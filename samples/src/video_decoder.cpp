// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "video_decoder.h"

extern "C"
{
#include <libavutil/imgutils.h>
}

#include <iostream>

static void throwOnFFmpegError(int result, char const *what)
{
    if (result < 0)
    {
        char message[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(result, message, sizeof(message));
        throw std::runtime_error(std::string(what) + ": " + message);
    }
}


VideoDecoder::VideoDecoder(std::string const &path)
{
    AVFormatContext *formatCtx = nullptr;
    throwOnFFmpegError(avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr), "Failed to open video");
    m_formatCtx.reset(formatCtx);

    throwOnFFmpegError(avformat_find_stream_info(formatCtx, nullptr), "Failed to find stream info");

    AVCodec const *codec = nullptr;
    m_streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    throwOnFFmpegError(m_streamIndex, "Failed to find a video stream");
    m_stream = formatCtx->streams[m_streamIndex];

    m_codecCtx.reset(avcodec_alloc_context3(codec));
    if (!m_codecCtx)
    {
        throw std::runtime_error("Failed to allocate the video decoder context");
    }
    throwOnFFmpegError(avcodec_parameters_to_context(m_codecCtx.get(), m_stream->codecpar), "Failed to copy codec parameters");
    throwOnFFmpegError(avcodec_open2(m_codecCtx.get(), codec, nullptr), "Failed to open the video codec");

    m_extent = vk::Extent2D(uint32_t(m_codecCtx->width), uint32_t(m_codecCtx->height));

    AVRational frameRate = av_guess_frame_rate(formatCtx, m_stream, nullptr);
    m_frameDuration = (frameRate.num > 0 && frameRate.den > 0)
        ? std::chrono::microseconds(av_rescale(1000000, frameRate.den, frameRate.num))
        : std::chrono::microseconds(16666);

    m_packet.reset(av_packet_alloc());
    m_next.reset(av_frame_alloc());
    m_current.reset(av_frame_alloc());
    m_converted.reset(av_frame_alloc());
    if (!m_packet || !m_next || !m_current || !m_converted)
    {
        throw std::runtime_error("Failed to allocate FFmpeg frames");
    }

    m_converted->format = AV_PIX_FMT_YUV420P;
    m_converted->width = m_codecCtx->width;
    m_converted->height = m_codecCtx->height;
    throwOnFFmpegError(av_frame_get_buffer(m_converted.get(), 0), "Failed to allocate the conversion frame");

    decodeNext();
}

VideoDecoder::~VideoDecoder()
{
    sws_freeContext(m_swsCtx);
}

float VideoDecoder::aspectRatio() const
{
    AVRational sar = av_guess_sample_aspect_ratio(m_formatCtx.get(), m_stream, nullptr);
    float pixelAspect = (sar.num > 0 && sar.den > 0) ? float(av_q2d(sar)) : 1.0f;
    return pixelAspect * float(m_extent.width) / float(m_extent.height);
}

void VideoDecoder::advance()
{
    av_frame_unref(m_current.get());
    av_frame_move_ref(m_current.get(), m_next.get());
    m_currentTime = m_nextTime;
    ++m_serial;

    if (m_current->format != AV_PIX_FMT_YUV420P)
    {
        convertCurrent();
    }

    decodeNext();
}

void VideoDecoder::rewind()
{
    int64_t start = (m_stream->start_time != AV_NOPTS_VALUE) ? m_stream->start_time : 0;
    throwOnFFmpegError(av_seek_frame(m_formatCtx.get(), m_streamIndex, start, AVSEEK_FLAG_BACKWARD), "Failed to seek");
    avcodec_flush_buffers(m_codecCtx.get());
    m_draining = false;
    m_decodedCount = 0;
    decodeNext();
}

AVFrame const *VideoDecoder::currentFrame() const
{
    if (m_serial == 0)
    {
        return nullptr;
    }
    return (m_current->format == AV_PIX_FMT_YUV420P) ? m_current.get() : m_converted.get();
}

void VideoDecoder::decodeNext()
{
    m_hasNext = false;

    while (true)
    {
        int result = avcodec_receive_frame(m_codecCtx.get(), m_next.get());
        if (result == 0)
        {
            m_nextTime = frameTime(*m_next);
            m_hasNext = true;
            ++m_decodedCount;
            return;
        }
        if (result == AVERROR_EOF)
        {
            return;
        }
        if (result != AVERROR(EAGAIN))
        {
            throwOnFFmpegError(result, "Failed to decode a video frame");
        }

        // The decoder needs more input.
        if (m_draining)
        {
            return;
        }

        result = av_read_frame(m_formatCtx.get(), m_packet.get());
        if (result < 0)
        {
            // End of file: send a null packet so the decoder flushes its remaining frames.
            avcodec_send_packet(m_codecCtx.get(), nullptr);
            m_draining = true;
            continue;
        }

        if (m_packet->stream_index == m_streamIndex)
        {
            result = avcodec_send_packet(m_codecCtx.get(), m_packet.get());
            if (result < 0 && result != AVERROR(EAGAIN))
            {
                // Skip corrupt packets rather than stopping playback.
                std::cerr << "videoplayer: dropping a packet the decoder rejected\n";
            }
        }
        av_packet_unref(m_packet.get());
    }
}

std::chrono::microseconds VideoDecoder::frameTime(AVFrame const &frame) const
{
    int64_t pts = frame.best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE)
    {
        return m_decodedCount * m_frameDuration;
    }
    if (m_stream->start_time != AV_NOPTS_VALUE)
    {
        pts -= m_stream->start_time;
    }
    return std::chrono::microseconds(av_rescale_q(pts, m_stream->time_base, AVRational{1, 1000000}));
}

void VideoDecoder::convertCurrent()
{
    m_swsCtx = sws_getCachedContext(
        m_swsCtx,
        m_current->width, m_current->height, AVPixelFormat(m_current->format),
        m_converted->width, m_converted->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx)
    {
        throw std::runtime_error("Failed to create the pixel format converter");
    }
    throwOnFFmpegError(av_frame_make_writable(m_converted.get()), "Failed to make the conversion frame writable");
    sws_scale(
        m_swsCtx,
        m_current->data, m_current->linesize, 0, m_current->height,
        m_converted->data, m_converted->linesize);
}


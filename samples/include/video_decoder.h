// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

// Decodes the video stream of a file into YUV 4:2:0 frames, converting other pixel formats.
// It keeps one frame of lookahead so the caller can check when the next frame is due before showing it.
class VideoDecoder
{
public:
    explicit VideoDecoder(std::string const &path);
    VideoDecoder(VideoDecoder const &) = delete;

    ~VideoDecoder();

    vk::Extent2D extent() const { return m_extent; }

    // Width over height of the displayed picture, allowing for non-square pixels.
    float aspectRatio() const;

    std::chrono::microseconds frameDuration() const { return m_frameDuration; }

    bool hasNextFrame() const { return m_hasNext; }
    std::chrono::microseconds nextFrameTime() const { return m_nextTime; }

    // Makes the next frame current and decodes the one after it.
    void advance();

    // Seeks back to the start of the stream for looping.
    void rewind();

    // The frame to display, always in AV_PIX_FMT_YUV420P.  Null until advance() is first called.
    AVFrame const *currentFrame() const;

    std::chrono::microseconds currentFrameTime() const { return m_currentTime; }

    // Increments every time the current frame changes, so consumers can tell when to re-upload.
    uint64_t serial() const { return m_serial; }

private:
    void decodeNext();

    // Presentation time relative to the start of the stream.
    std::chrono::microseconds frameTime(AVFrame const &frame) const;

    void convertCurrent();

    struct FormatContextDeleter { void operator()(AVFormatContext *p) const { avformat_close_input(&p); } };
    struct CodecContextDeleter { void operator()(AVCodecContext *p) const { avcodec_free_context(&p); } };
    struct PacketDeleter { void operator()(AVPacket *p) const { av_packet_free(&p); } };
    struct FrameDeleter { void operator()(AVFrame *p) const { av_frame_free(&p); } };

    std::unique_ptr<AVFormatContext, FormatContextDeleter> m_formatCtx;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> m_codecCtx;
    std::unique_ptr<AVPacket, PacketDeleter> m_packet;
    std::unique_ptr<AVFrame, FrameDeleter> m_next;
    std::unique_ptr<AVFrame, FrameDeleter> m_current;
    std::unique_ptr<AVFrame, FrameDeleter> m_converted;
    SwsContext *m_swsCtx{nullptr};

    AVStream *m_stream{nullptr};
    int m_streamIndex{-1};
    vk::Extent2D m_extent;
    std::chrono::microseconds m_frameDuration{};

    bool m_hasNext{false};
    bool m_draining{false};
    int64_t m_decodedCount{0};
    std::chrono::microseconds m_nextTime{};
    std::chrono::microseconds m_currentTime{};
    uint64_t m_serial{0};
};

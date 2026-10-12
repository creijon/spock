// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "vulkan_video_decoder.h"

#include "spock/foundry.hpp"

#include <iostream>
#include <set>
#include <stdexcept>

static void throwOnFFmpegError(int result, char const *what)
{
    if (result < 0)
    {
        char message[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(result, message, sizeof(message));
        throw std::runtime_error(std::string(what) + ": " + message);
    }
}

namespace
{
    struct BufferRefDeleter { void operator()(AVBufferRef *p) const { av_buffer_unref(&p); } };

    using BufferRefPtr = std::unique_ptr<AVBufferRef, BufferRefDeleter>;

    // FFmpeg keeps pointers to the extension names and features, and uses the device until the last frame
    // it decoded is freed, which can be after the decoder has gone.  So FFmpeg's device context owns them,
    // along with a reference to the Foundry that keeps the device alive.
    struct DeviceContextData
    {
        spock::FoundryPtr foundry;
        std::vector<char const *> extensions;
        VulkanVideoDecoder::DeviceFeatures features;
    };

    // The Vulkan Video extension that decodes a codec, or null if there isn't one.
    char const *decodeExtension(AVCodecID codecId)
    {
        switch (codecId)
        {
        case AV_CODEC_ID_H264: return VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
        case AV_CODEC_ID_HEVC: return VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
        case AV_CODEC_ID_AV1: return VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME;
        case AV_CODEC_ID_VP9: return VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME;
        default: return nullptr;
        }
    }

    // The default decoder for a codec can be a software-only library, such as libdav1d for AV1,
    // so look for one with a Vulkan hwaccel.
    AVCodec const *findVulkanDecoder(AVCodecID codecId)
    {
        void *iterator = nullptr;
        while (AVCodec const *codec = av_codec_iterate(&iterator))
        {
            if (codec->id != codecId || !av_codec_is_decoder(codec))
            {
                continue;
            }

            for (int i = 0; AVCodecHWConfig const *config = avcodec_get_hw_config(codec, i); ++i)
            {
                if (config->device_type == AV_HWDEVICE_TYPE_VULKAN &&
                    (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
                {
                    return codec;
                }
            }
        }
        return nullptr;
    }

    // Wraps the Foundry's device in an FFmpeg Vulkan device context, rather than letting FFmpeg create its own,
    // so the decoded images can be used directly by the renderer.
    BufferRefPtr createDeviceContext(
        vk::raii::Instance const &instance,
        PFN_vkGetInstanceProcAddr getInstanceProcAddr,
        spock::FoundryPtr const &foundry)
    {
        BufferRefPtr deviceRef(av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN));
        if (!deviceRef)
        {
            throw std::runtime_error("Failed to allocate an FFmpeg Vulkan device context");
        }

        auto data = std::make_unique<DeviceContextData>(
            DeviceContextData{foundry, {}, VulkanVideoDecoder::requiredDeviceFeatures()});
        for (std::string const &extension : foundry->extensions())
        {
            data->extensions.push_back(extension.c_str());
        }

        auto *deviceCtx = reinterpret_cast<AVHWDeviceContext *>(deviceRef->data);
        auto *vkCtx = static_cast<AVVulkanDeviceContext *>(deviceCtx->hwctx);

        vkCtx->get_proc_addr = getInstanceProcAddr;
        vkCtx->inst = *instance;
        vkCtx->phys_dev = *foundry->physicalDevice();
        vkCtx->act_dev = *foundry->device();
        vkCtx->device_features = data->features.get<vk::PhysicalDeviceFeatures2>();
        vkCtx->enabled_dev_extensions = data->extensions.data();
        vkCtx->nb_enabled_dev_extensions = static_cast<int>(data->extensions.size());

        // FFmpeg only uses instance extensions for optional extras, so none are listed.
        vkCtx->enabled_inst_extensions = nullptr;
        vkCtx->nb_enabled_inst_extensions = 0;

        // List the families the Foundry created a queue in.  FFmpeg creates its images with concurrent sharing
        // across all of them, so they can be sampled on the graphics queue without an ownership transfer.
        // FFmpeg fills in the codecs each video family supports.
        spock::QueueFamilies const &families = foundry->queueFamilies();
        std::vector<vk::QueueFamilyProperties> properties = foundry->physicalDevice().getQueueFamilyProperties();
        for (uint32_t family : std::set<uint32_t>{families.graphics, families.compute, families.transfer, families.videoDecode})
        {
            vkCtx->qf[vkCtx->nb_qf++] = AVVulkanDeviceQueueFamily{
                .idx = static_cast<int>(family),
                .num = 1,
                .flags = static_cast<VkQueueFlagBits>(static_cast<VkQueueFlags>(properties[family].queueFlags)),
            };
        }

        // From here the device context frees the data, even if initialization fails.
        deviceCtx->user_opaque = data.release();
        deviceCtx->free = [](AVHWDeviceContext *ctx) { delete static_cast<DeviceContextData *>(ctx->user_opaque); };

        throwOnFFmpegError(av_hwdevice_ctx_init(deviceRef.get()), "Failed to initialize the FFmpeg Vulkan device context");
        return deviceRef;
    }
}

VulkanVideoDecoder::DeviceFeatures VulkanVideoDecoder::requiredDeviceFeatures()
{
    // FFmpeg views its multi-planar images through YCbCr conversions, synchronizes every frame with
    // timeline semaphores, and records its commands with synchronization2.
    DeviceFeatures features;
    features.get<vk::PhysicalDeviceVulkan11Features>().samplerYcbcrConversion = VK_TRUE;
    features.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore = VK_TRUE;
    features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;
    return features;
}

VulkanVideoDecoder::VulkanVideoDecoder(std::string const &path)
{
    AVFormatContext *formatCtx = nullptr;
    throwOnFFmpegError(avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr), "Failed to open video");
    m_formatCtx.reset(formatCtx);

    throwOnFFmpegError(avformat_find_stream_info(formatCtx, nullptr), "Failed to find stream info");

    m_streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    throwOnFFmpegError(m_streamIndex, "Failed to find a video stream");
    m_stream = formatCtx->streams[m_streamIndex];

    AVCodecID codecId = m_stream->codecpar->codec_id;
    m_codec = decodeExtension(codecId) ? findVulkanDecoder(codecId) : nullptr;
    if (!m_codec)
    {
        throw std::runtime_error(std::string("FFmpeg can't decode ") + avcodec_get_name(codecId) + " with Vulkan Video");
    }

    m_extent = vk::Extent2D(uint32_t(m_stream->codecpar->width), uint32_t(m_stream->codecpar->height));

    AVRational frameRate = av_guess_frame_rate(formatCtx, m_stream, nullptr);
    m_frameDuration = (frameRate.num > 0 && frameRate.den > 0)
        ? std::chrono::microseconds(av_rescale(1000000, frameRate.den, frameRate.num))
        : std::chrono::microseconds(16666);
}

std::vector<char const *> VulkanVideoDecoder::requiredDeviceExtensions() const
{
    return {
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        decodeExtension(m_stream->codecpar->codec_id)};
}

void VulkanVideoDecoder::start(
    vk::raii::Instance const &instance,
    PFN_vkGetInstanceProcAddr getInstanceProcAddr,
    spock::FoundryPtr const &foundry)
{
    BufferRefPtr deviceRef = createDeviceContext(instance, getInstanceProcAddr, foundry);

    m_codecCtx.reset(avcodec_alloc_context3(m_codec));
    if (!m_codecCtx)
    {
        throw std::runtime_error("Failed to allocate the video decoder context");
    }
    throwOnFFmpegError(avcodec_parameters_to_context(m_codecCtx.get(), m_stream->codecpar), "Failed to copy codec parameters");

    m_codecCtx->hw_device_ctx = av_buffer_ref(deviceRef.get());
    if (!m_codecCtx->hw_device_ctx)
    {
        throw std::runtime_error("Failed to reference the FFmpeg Vulkan device context");
    }
    m_codecCtx->opaque = this;
    m_codecCtx->get_format = selectVulkanFormat;

    // The renderer updates a frame's semaphore value and layout while it records, and submits afterwards.
    // FFmpeg's frame threads could use the same frame as a reference in between, so decode on this thread.
    m_codecCtx->thread_count = 1;

    throwOnFFmpegError(avcodec_open2(m_codecCtx.get(), m_codec, nullptr), "Failed to open the video codec");

    m_packet.reset(av_packet_alloc());
    m_next.reset(av_frame_alloc());
    m_current.reset(av_frame_alloc());
    if (!m_packet || !m_next || !m_current)
    {
        throw std::runtime_error("Failed to allocate FFmpeg frames");
    }

    decodeNext();
}

AVPixelFormat VulkanVideoDecoder::selectVulkanFormat(AVCodecContext *codecCtx, AVPixelFormat const *formats)
{
    for (; *formats != AV_PIX_FMT_NONE; ++formats)
    {
        if (*formats == AV_PIX_FMT_VULKAN)
        {
            return AV_PIX_FMT_VULKAN;
        }
    }

    // FFmpeg asks again without Vulkan when its hwaccel fails to start, such as for a profile the device
    // doesn't support.  Only Vulkan frames can be displayed, so stop rather than fall back to software.
    static_cast<VulkanVideoDecoder *>(codecCtx->opaque)->m_unsupported = true;
    return AV_PIX_FMT_NONE;
}

void VulkanVideoDecoder::throwIfUnsupported() const
{
    if (m_unsupported)
    {
        throw std::runtime_error("The device can't decode this stream with Vulkan Video, FFmpeg's log has the reason");
    }
}

float VulkanVideoDecoder::aspectRatio() const
{
    AVRational sar = av_guess_sample_aspect_ratio(m_formatCtx.get(), m_stream, nullptr);
    float pixelAspect = (sar.num > 0 && sar.den > 0) ? float(av_q2d(sar)) : 1.0f;
    return pixelAspect * float(m_extent.width) / float(m_extent.height);
}

void VulkanVideoDecoder::advance()
{
    av_frame_unref(m_current.get());
    av_frame_move_ref(m_current.get(), m_next.get());
    m_currentTime = m_nextTime;
    ++m_serial;

    decodeNext();
}

void VulkanVideoDecoder::rewind()
{
    int64_t start = (m_stream->start_time != AV_NOPTS_VALUE) ? m_stream->start_time : 0;
    throwOnFFmpegError(av_seek_frame(m_formatCtx.get(), m_streamIndex, start, AVSEEK_FLAG_BACKWARD), "Failed to seek");
    avcodec_flush_buffers(m_codecCtx.get());
    m_draining = false;
    m_decodedCount = 0;
    decodeNext();
}

AVFrame const *VulkanVideoDecoder::currentFrame() const
{
    return (m_serial == 0) ? nullptr : m_current.get();
}

void VulkanVideoDecoder::decodeNext()
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
            throwIfUnsupported();
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
                throwIfUnsupported();

                // Skip corrupt packets rather than stopping playback.
                std::cerr << "vulkan_video_player: dropping a packet the decoder rejected\n";
            }
        }
        av_packet_unref(m_packet.get());
    }
}

std::chrono::microseconds VulkanVideoDecoder::frameTime(AVFrame const &frame) const
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

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "spock/types.hpp"

#include <vulkan/vulkan_raii.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext_vulkan.h>
}

#include <chrono>
#include <memory>
#include <string>
#include <vector>

// Decodes the video stream of a file with Vulkan Video, through FFmpeg's Vulkan hwaccel on spock's device.
// The frames stay on the GPU as AVVkFrames, so the caller can sample their images directly.
// Like VideoDecoder, it keeps one frame of lookahead so the caller can check when the next frame is due.
//
// FFmpeg needs the device before it can decode, so it is created in two steps:
// the constructor opens the file to find the extensions the device needs, and start() begins decoding on it.
class VulkanVideoDecoder
{
public:
    // The features FFmpeg's Vulkan code relies on, chained for vk::DeviceCreateInfo.
    using DeviceFeatures = vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features>;

    static DeviceFeatures requiredDeviceFeatures();

    explicit VulkanVideoDecoder(std::string const &path);
    VulkanVideoDecoder(VulkanVideoDecoder const &) = delete;

    // The device extensions needed to decode this file's codec.
    std::vector<char const *> requiredDeviceExtensions() const;

    // Opens the decoder on the foundry's device and decodes the first frame.
    // The foundry needs a video decode queue, requiredDeviceExtensions() and requiredDeviceFeatures().
    // Frames are submitted to the device's queues from the calling thread, so call everything on the render thread.
    void start(
        vk::raii::Instance const &instance,
        PFN_vkGetInstanceProcAddr getInstanceProcAddr,
        spock::FoundryPtr const &foundry);

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

    // The frame to display, always in AV_PIX_FMT_VULKAN.  Null until advance() is first called.
    // Its AVVkFrame must be used following FFmpeg's synchronization rules, see AVVkFrame::sem.
    AVFrame const *currentFrame() const;

    std::chrono::microseconds currentFrameTime() const { return m_currentTime; }

    // Increments every time the current frame changes, so consumers can tell when to switch images.
    uint64_t serial() const { return m_serial; }

private:
    // FFmpeg's get_format callback, which only accepts Vulkan frames.
    static AVPixelFormat selectVulkanFormat(AVCodecContext *codecCtx, AVPixelFormat const *formats);

    void throwIfUnsupported() const;

    void decodeNext();

    // Presentation time relative to the start of the stream.
    std::chrono::microseconds frameTime(AVFrame const &frame) const;

    struct FormatContextDeleter { void operator()(AVFormatContext *p) const { avformat_close_input(&p); } };
    struct CodecContextDeleter { void operator()(AVCodecContext *p) const { avcodec_free_context(&p); } };
    struct PacketDeleter { void operator()(AVPacket *p) const { av_packet_free(&p); } };
    struct FrameDeleter { void operator()(AVFrame *p) const { av_frame_free(&p); } };

    std::unique_ptr<AVFormatContext, FormatContextDeleter> m_formatCtx;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> m_codecCtx;
    std::unique_ptr<AVPacket, PacketDeleter> m_packet;
    std::unique_ptr<AVFrame, FrameDeleter> m_next;
    std::unique_ptr<AVFrame, FrameDeleter> m_current;

    AVCodec const *m_codec{nullptr};
    AVStream *m_stream{nullptr};
    int m_streamIndex{-1};
    vk::Extent2D m_extent;
    std::chrono::microseconds m_frameDuration{};

    // Set when FFmpeg can't decode the stream with Vulkan Video.
    bool m_unsupported{false};

    bool m_hasNext{false};
    bool m_draining{false};
    int64_t m_decodedCount{0};
    std::chrono::microseconds m_nextTime{};
    std::chrono::microseconds m_currentTime{};
    uint64_t m_serial{0};
};

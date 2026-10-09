// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// Plays a video file decoded by FFmpeg, based on the QUAD sample.
// Each decoded YUV 4:2:0 frame is written straight into persistently mapped, linearly tiled
// Y, U and V textures, which the fragment shader samples and converts to RGB.
// There is one set of textures per frame in flight, so the CPU never writes to a texture
// the GPU may still be reading.
//
// Usage: videoplayer <video file>

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct QuadVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32G32Sfloat, offsetof(QuadVertex, pos) }
        };
    }

    glm::vec2 pos;
};

static const QuadVertex QUAD_VERTEX_DATA[] =
{
    {{-1.0f, -1.0f}},
    {{ 1.0f, -1.0f}},
    {{-1.0f,  1.0f}},
    {{ 1.0f,  1.0f}},
};

static constexpr uint32_t QUAD_VERTEX_BUFFER_SIZE{sizeof(QUAD_VERTEX_DATA)};
static constexpr uint32_t QUAD_VERTEX_COUNT{std::size(QUAD_VERTEX_DATA)};

// Y, U and V.
static constexpr uint32_t PLANE_COUNT{3};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  mat4 yuvToRgb;
  vec2 scale;
} pc;

layout (location = 0) in vec2 pos;

layout (location = 0) out vec2 uv;

void main()
{
  uv = (pos + 1.0) * 0.5;
  gl_Position = vec4(pos * pc.scale, 0.0, 1.0);
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  mat4 yuvToRgb;
  vec2 scale;
} pc;

layout (binding = 0) uniform sampler2D yTex;
layout (binding = 1) uniform sampler2D uTex;
layout (binding = 2) uniform sampler2D vTex;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

void main()
{
  vec4 yuv = vec4(
    texture(yTex, uv).r,
    texture(uTex, uv).r,
    texture(vTex, uv).r,
    1.0);
  outColor = vec4((pc.yuvToRgb * yuv).rgb, 1.0);
}
)";

// Shared by both shader stages.
struct PushConstants
{
    glm::mat4 yuvToRgb;
    glm::vec2 scale;
};

// Maps (Y, U, V, 1) as sampled from the textures to (R, G, B, 1).
// BT.601 limited range, with the same coefficients as the ffmpeggl sample.
static glm::mat4 bt601LimitedRangeMatrix()
{
    // Remove the black level from Y and the midpoint from U and V.
    glm::mat4 offset(1.0f);
    offset[3] = glm::vec4(-0.0625f, -0.5f, -0.5f, 1.0f);

    // Each column is the contribution of Y, U and V respectively to RGB.
    glm::mat4 coefficients(
        glm::vec4(1.1640625f, 1.1640625f, 1.1640625f, 0.0f),
        glm::vec4(0.0f, -0.390625f, 2.015625f, 0.0f),
        glm::vec4(1.370705f, -0.8125f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    return coefficients * offset;
}

static void throwOnFFmpegError(int result, char const *what)
{
    if (result < 0)
    {
        char message[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(result, message, sizeof(message));
        throw std::runtime_error(std::string(what) + ": " + message);
    }
}

// Decodes the video stream of a file into YUV 4:2:0 frames, converting other pixel formats.
// It keeps one frame of lookahead so the caller can check when the next frame is due before showing it.
class VideoDecoder
{
public:
    explicit VideoDecoder(std::string const &path)
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

    VideoDecoder(VideoDecoder const &) = delete;

    ~VideoDecoder()
    {
        sws_freeContext(m_swsCtx);
    }

    vk::Extent2D extent() const { return m_extent; }

    // Width over height of the displayed picture, allowing for non-square pixels.
    float aspectRatio() const
    {
        AVRational sar = av_guess_sample_aspect_ratio(m_formatCtx.get(), m_stream, nullptr);
        float pixelAspect = (sar.num > 0 && sar.den > 0) ? float(av_q2d(sar)) : 1.0f;
        return pixelAspect * float(m_extent.width) / float(m_extent.height);
    }

    std::chrono::microseconds frameDuration() const { return m_frameDuration; }

    bool hasNextFrame() const { return m_hasNext; }
    std::chrono::microseconds nextFrameTime() const { return m_nextTime; }

    // Makes the next frame current and decodes the one after it.
    void advance()
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

    // Seeks back to the start of the stream for looping.
    void rewind()
    {
        int64_t start = (m_stream->start_time != AV_NOPTS_VALUE) ? m_stream->start_time : 0;
        throwOnFFmpegError(av_seek_frame(m_formatCtx.get(), m_streamIndex, start, AVSEEK_FLAG_BACKWARD), "Failed to seek");
        avcodec_flush_buffers(m_codecCtx.get());
        m_draining = false;
        m_decodedCount = 0;
        decodeNext();
    }

    // The frame to display, always in AV_PIX_FMT_YUV420P.  Null until advance() is first called.
    AVFrame const *currentFrame() const
    {
        if (m_serial == 0)
        {
            return nullptr;
        }
        return (m_current->format == AV_PIX_FMT_YUV420P) ? m_current.get() : m_converted.get();
    }

    std::chrono::microseconds currentFrameTime() const { return m_currentTime; }

    // Increments every time the current frame changes, so consumers can tell when to re-upload.
    uint64_t serial() const { return m_serial; }

private:
    void decodeNext()
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

    // Presentation time relative to the start of the stream.
    std::chrono::microseconds frameTime(AVFrame const &frame) const
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

    void convertCurrent()
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

// A single 8 bit plane of a video frame, held in a linearly tiled image that stays mapped for its lifetime.
// Host writes to a linear image are only well defined in the eGeneral or ePreinitialized layouts,
// so the image is kept in eGeneral and sampled from there.
struct VideoPlane
{
    VideoPlane(
        spock::FoundryPtr const &foundry,
        vk::Extent2D planeExtent)
        : extent(planeExtent)
        , image(
            foundry,
            vk::Format::eR8Unorm,
            planeExtent,
            vk::ImageTiling::eLinear,
            vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::ePreinitialized,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            vk::ImageAspectFlagBits::eColor)
    {
        vk::SubresourceLayout layout = image.image().getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});
        rowPitch = layout.rowPitch;
        mapped = static_cast<uint8_t *>(image.deviceMemory().mapMemory(0, VK_WHOLE_SIZE)) + layout.offset;
    }

    // Copies a plane of a frame row by row, as the image row pitch rarely matches the FFmpeg line size.
    void upload(uint8_t const *source, int lineSize) const
    {
        for (uint32_t row = 0; row < extent.height; ++row)
        {
            std::memcpy(mapped + row * rowPitch, source + ptrdiff_t(row) * lineSize, extent.width);
        }
    }

    vk::Extent2D extent;
    spock::ImageWrapper image;
    vk::DeviceSize rowPitch{0};

    // Freeing the device memory implicitly unmaps it.
    uint8_t *mapped{nullptr};
};

// The textures and descriptor set for one frame in flight.
struct FrameResources
{
    std::vector<VideoPlane> planes;
    vk::raii::DescriptorSet descriptorSet{nullptr};

    // The decoder serial of the frame held in the planes, zero if none.
    uint64_t serial{0};
};

class VideoRenderer : public spock::Renderer
{
public:
    VideoRenderer(
        spock::FoundryPtr const &foundry,
        vk::Extent2D const& extents,
        std::shared_ptr<const VideoDecoder> const &decoder)
        : spock::Renderer(
            foundry,
            extents,
            {0.0f, 0.0f, 0.0f, 1.0},
            {1.0f, 0},
            false)
        , m_decoder(decoder)
    {
        checkFormatSupport();

        m_vertexBuffer = spock::BufferWrapper(
            foundry,
            QUAD_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), QUAD_VERTEX_DATA, QUAD_VERTEX_COUNT);

        m_sampler = spock::createSampler(
            foundry->device(),
            vk::Filter::eLinear,
            vk::SamplerAddressMode::eClampToEdge);

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            foundry->device(),
            vk::ShaderStageFlagBits::eFragment,
            std::vector<vk::DescriptorType>(PLANE_COUNT, vk::DescriptorType::eCombinedImageSampler));

        vk::PushConstantRange pushConstantRange{
            PUSH_CONSTANT_STAGES,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = vk::raii::PipelineLayout(foundry->device(), {{}, *m_descriptorSetLayout, pushConstantRange});

        m_descriptorPool = spock::createDescriptorPool(
            foundry->device(),
            { {vk::DescriptorType::eCombinedImageSampler, PLANE_COUNT * m_framesInFlight} });

        createFrameResources();
        createPipeline();
    }

protected:
    void checkFormatSupport() const
    {
        vk::FormatFeatureFlags required =
            vk::FormatFeatureFlagBits::eSampledImage | vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
        vk::FormatProperties properties = m_foundry->physicalDevice().getFormatProperties(vk::Format::eR8Unorm);
        if ((properties.linearTilingFeatures & required) != required)
        {
            throw std::runtime_error("The device can't sample linearly tiled R8 images, which this sample requires");
        }
    }

    void createFrameResources()
    {
        vk::Extent2D lumaExtent = m_decoder->extent();
        vk::Extent2D chromaExtent((lumaExtent.width + 1) / 2, (lumaExtent.height + 1) / 2);

        std::vector<vk::DescriptorSetLayout> layouts(m_framesInFlight, *m_descriptorSetLayout);
        vk::raii::DescriptorSets descriptorSets(m_foundry->device(), {m_descriptorPool, layouts});

        m_frames.resize(m_framesInFlight);
        for (uint32_t i = 0; i < m_framesInFlight; ++i)
        {
            FrameResources &frame = m_frames[i];
            frame.planes.reserve(PLANE_COUNT);
            frame.planes.emplace_back(m_foundry, lumaExtent);
            frame.planes.emplace_back(m_foundry, chromaExtent);
            frame.planes.emplace_back(m_foundry, chromaExtent);
            frame.descriptorSet = std::move(descriptorSets[i]);

            // Start black rather than with undefined contents.
            std::memset(frame.planes[0].mapped, 0, frame.planes[0].rowPitch * lumaExtent.height);
            std::memset(frame.planes[1].mapped, 128, frame.planes[1].rowPitch * chromaExtent.height);
            std::memset(frame.planes[2].mapped, 128, frame.planes[2].rowPitch * chromaExtent.height);

            std::array<vk::DescriptorImageInfo, PLANE_COUNT> imageInfos;
            std::array<vk::WriteDescriptorSet, PLANE_COUNT> writes;
            for (uint32_t p = 0; p < PLANE_COUNT; ++p)
            {
                imageInfos[p] = vk::DescriptorImageInfo(*m_sampler, *frame.planes[p].image.imageView(), vk::ImageLayout::eGeneral);
                writes[p] = vk::WriteDescriptorSet(*frame.descriptorSet, p, 0, vk::DescriptorType::eCombinedImageSampler, imageInfos[p]);
            }
            m_foundry->device().updateDescriptorSets(writes, nullptr);
        }

        m_foundry->submit(
            [&](vk::CommandBuffer commandBuffer)
            {
                for (FrameResources const &frame : m_frames)
                {
                    for (VideoPlane const &plane : frame.planes)
                    {
                        spock::setImageLayout(
                            commandBuffer,
                            plane.image.image(),
                            plane.image.format(),
                            vk::ImageLayout::ePreinitialized,
                            vk::ImageLayout::eGeneral);
                    }
                }
            });
    }

    void createPipeline()
    {
        // Create the shaders.
        auto vertShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
        auto fragShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
            {{}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main"},
        };

        // Finally create the graphics pipeline.
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_foundry->device(),
            shaderStagesInfo,
            m_pipelineLayout,
            m_renderPass,
            spock::VertexFormatWrapper<QuadVertex>(),
            vk::PrimitiveTopology::eTriangleStrip,
            vk::CullModeFlagBits::eBack,
            false);
    }

    void render(spock::FrameState &frame) override
    {
        // The presenter has waited for this frame's fence, so the GPU is no longer reading its textures.
        FrameResources &decodedFrame = m_frames[m_inFlightIndex];
        AVFrame const *videoFrame = m_decoder->currentFrame();
        if (videoFrame && decodedFrame.serial != m_decoder->serial())
        {
            for (uint32_t p = 0; p < PLANE_COUNT; ++p)
            {
                decodedFrame.planes[p].upload(videoFrame->data[p], videoFrame->linesize[p]);
            }
            decodedFrame.serial = m_decoder->serial();
        }

        // Bind the pipeline and vertex buffers.
        auto& commandBuffer = frame.commandBuffer;

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {decodedFrame.descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Letterbox or pillbox the video.
        float windowAspect = float(m_extents.width) / float(m_extents.height);
        float videoAspect = m_decoder->aspectRatio();
        glm::vec2 scale{1.0f};
        scale.x = (windowAspect > videoAspect) ? (videoAspect / windowAspect) : 1.0f;
        scale.y = (windowAspect < videoAspect) ? (windowAspect / videoAspect) : 1.0f;

        PushConstants pushConstants{m_yuvToRgb, scale};
        spock::pushConstants(commandBuffer, m_pipelineLayout, PUSH_CONSTANT_STAGES, pushConstants);

        commandBuffer.draw(QUAD_VERTEX_COUNT, 1, 0, 0);
    }

private:
    static constexpr vk::ShaderStageFlags PUSH_CONSTANT_STAGES{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment};

    std::shared_ptr<const VideoDecoder> m_decoder;
    glm::mat4 m_yuvToRgb{bt601LimitedRangeMatrix()};

    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::Sampler m_sampler{nullptr};

    // Declared after the pool so the descriptor sets are freed first.
    std::vector<FrameResources> m_frames;

    spock::BufferWrapper m_vertexBuffer;
};

class VideoPlayerApp : public spock::App
{
public:
    explicit VideoPlayerApp(std::string const &path)
        : VideoPlayerApp(std::make_shared<VideoDecoder>(path))
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        return std::make_unique<VideoRenderer>(m_foundry, m_window.extents(), m_decoder);
    }

    void update() override
    {
        std::chrono::microseconds playbackTime = m_time - m_playbackStart;

        // Show the latest frame that is due, dropping any the app loop was too slow to display.
        while (m_decoder->hasNextFrame() && m_decoder->nextFrameTime() <= playbackTime)
        {
            m_decoder->advance();
        }

        // Once the last frame has been on screen for its duration, loop back to the start.
        if (!m_decoder->hasNextFrame() &&
            playbackTime >= m_decoder->currentFrameTime() + m_decoder->frameDuration())
        {
            m_decoder->rewind();
            m_playbackStart = m_time;
            if (m_decoder->hasNextFrame())
            {
                m_decoder->advance();
            }
        }
    }

private:
    explicit VideoPlayerApp(std::shared_ptr<VideoDecoder> decoder)
        : spock::App(
            "Video Player",
            decoder->extent().width,
            decoder->extent().height)
        , m_decoder(std::move(decoder))
    {
    }

    std::shared_ptr<VideoDecoder> m_decoder;
    std::chrono::microseconds m_playbackStart{0};
};

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video file>\n";
        return 1;
    }

    return spock::runApp<VideoPlayerApp>(std::string(argv[1]));
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// Plays a video file decoded on the GPU with Vulkan Video, based on the video_player sample.
// FFmpeg's Vulkan hwaccel decodes on spock's device, so each frame stays in the image it was decoded into.
// The fragment shader samples the luma and chroma planes of that image through separate image views, then
// converts to RGB as video_player does.
//
// Usage: vulkan_video_player <video file>
// Mesa's Intel driver only exposes its video queues when run with ANV_DEBUG=video-decode.

#include "vulkan_video_decoder.h"

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
#include <libavutil/pixdesc.h>
}

#include <array>
#include <chrono>
#include <cstddef>
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

// Luma, then the interleaved chroma of a semi-planar format such as NV12 or P010.
static constexpr uint32_t PLANE_COUNT{2};
static constexpr std::array<vk::ImageAspectFlagBits, PLANE_COUNT> PLANE_ASPECTS{
    vk::ImageAspectFlagBits::ePlane0,
    vk::ImageAspectFlagBits::ePlane1};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  mat4 yuvToRgb;
  vec2 scale;
  vec2 uvScale;
} pc;

layout (location = 0) in vec2 pos;

layout (location = 0) out vec2 uv;

void main()
{
  uv = (pos + 1.0) * 0.5 * pc.uvScale;
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
  vec2 uvScale;
} pc;

layout (binding = 0) uniform sampler2D lumaTex;
layout (binding = 1) uniform sampler2D chromaTex;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

void main()
{
  vec4 yuv = vec4(
    texture(lumaTex, uv).r,
    texture(chromaTex, uv).rg,
    1.0);
  outColor = vec4((pc.yuvToRgb * yuv).rgb, 1.0);
}
)";

// Shared by both shader stages.
struct PushConstants
{
    glm::mat4 yuvToRgb;
    glm::vec2 scale;
    // The part of the image to sample, as the decoded image can be larger than the picture.
    glm::vec2 uvScale;
};

// Maps (Y, U, V, 1) as sampled from the textures to (R, G, B, 1).
// BT.601 limited range, with the same coefficients as the video_player sample.
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

struct FrameDeleter { void operator()(AVFrame *p) const { av_frame_free(&p); } };

// The decoded frame shown by one frame in flight, and the image views and descriptor set that sample it.
class VideoFrameState : public spock::FrameState
{
public:
    VideoFrameState(
        spock::FoundryPtr const &foundry,
        vk::raii::DescriptorPool const &descriptorPool,
        vk::raii::DescriptorSetLayout const &descriptorSetLayout)
        : spock::FrameState(foundry->device(), foundry->commandPool())
        , videoFrame(av_frame_alloc())
        , descriptorSet(std::move(vk::raii::DescriptorSets(foundry->device(), {descriptorPool, *descriptorSetLayout}).front()))
    {
        if (!videoFrame)
        {
            throw std::runtime_error("Failed to allocate an FFmpeg frame");
        }
    }

    // Switches to a newly decoded frame and points the descriptor set at its planes.
    void show(
        spock::FoundryPtr const &foundry,
        vk::raii::Sampler const &sampler,
        AVFrame const *frame,
        uint64_t frameSerial)
    {
        // Destroy the views before releasing the frame that keeps their image alive.
        for (vk::raii::ImageView &view : planeViews)
        {
            view = nullptr;
        }
        av_frame_unref(videoFrame.get());
        if (av_frame_ref(videoFrame.get(), frame) < 0)
        {
            throw std::runtime_error("Failed to reference a decoded frame");
        }

        auto const *framesCtx = reinterpret_cast<AVHWFramesContext const *>(videoFrame->hw_frames_ctx->data);
        VkFormat const *planeFormats = av_vkfmt_from_pixfmt(framesCtx->sw_format);
        AVVkFrame const *vkFrame = vulkanFrame();

        // Vulkan Video decodes into a single multi-planar image with two planes.
        if (!planeFormats ||
            planeFormats[1] == VK_FORMAT_UNDEFINED ||
            planeFormats[2] != VK_FORMAT_UNDEFINED ||
            vkFrame->img[1] != VK_NULL_HANDLE)
        {
            throw std::runtime_error(std::string("Unsupported decoded format: ") + av_get_pix_fmt_name(framesCtx->sw_format));
        }

        // FFmpeg creates the images with a mutable format, so each plane can be viewed with its own format.
        // The views are only sampled, which the plane formats support even though the video usages aren't.
        vk::ImageViewUsageCreateInfo viewUsage(vk::ImageUsageFlagBits::eSampled);
        std::array<vk::DescriptorImageInfo, PLANE_COUNT> imageInfos;
        std::array<vk::WriteDescriptorSet, PLANE_COUNT> writes;
        for (uint32_t p = 0; p < PLANE_COUNT; ++p)
        {
            vk::ImageViewCreateInfo viewInfo(
                {},
                vk::Image(vkFrame->img[0]),
                vk::ImageViewType::e2D,
                vk::Format(planeFormats[p]),
                {},
                {PLANE_ASPECTS[p], 0, 1, 0, 1},
                &viewUsage);
            planeViews[p] = vk::raii::ImageView(foundry->device(), viewInfo);

            imageInfos[p] = vk::DescriptorImageInfo(*sampler, *planeViews[p], vk::ImageLayout::eShaderReadOnlyOptimal);
            writes[p] = vk::WriteDescriptorSet(*descriptorSet, p, 0, vk::DescriptorType::eCombinedImageSampler, imageInfos[p]);
        }
        foundry->device().updateDescriptorSets(writes, nullptr);

        // The image has the coded size, which is padded to whole macroblocks, so only sample the picture.
        uvScale = glm::vec2(
            float(videoFrame->width) / float(framesCtx->width),
            float(videoFrame->height) / float(framesCtx->height));
        serial = frameSerial;
    }

    AVVkFrame *vulkanFrame() const
    {
        return reinterpret_cast<AVVkFrame *>(videoFrame->data[0]);
    }

    // Keeps the decoded image alive until this frame state is reused, by when the GPU has finished with it.
    std::unique_ptr<AVFrame, FrameDeleter> videoFrame;
    std::array<vk::raii::ImageView, PLANE_COUNT> planeViews{nullptr, nullptr};
    vk::raii::DescriptorSet descriptorSet{nullptr};
    glm::vec2 uvScale{1.0f};

    // The decoder serial of the frame being shown, zero if none.
    uint64_t serial{0};
};

class VulkanVideoRenderer : public spock::Renderer
{
public:
    VulkanVideoRenderer(
        spock::FoundryPtr const &foundry,
        vk::Extent2D const& extents,
        std::shared_ptr<const VulkanVideoDecoder> const &decoder)
        : spock::Renderer(
            foundry,
            extents,
            {0.0f, 0.0f, 0.0f, 1.0},
            {1.0f, 0},
            false)
        , m_decoder(decoder)
    {
        // Frames are created on the first renderFrame, once the descriptor pool and layout exist.
        m_createFrameFunc = [this](spock::FoundryPtr const& foundry)
        {
            return std::make_unique<VideoFrameState>(foundry, m_descriptorPool, m_descriptorSetLayout);
        };

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

        createPipeline();
    }

    ~VulkanVideoRenderer() override
    {
        // The frames' descriptor sets are freed back to m_descriptorPool, so release them while it still exists.
        m_framePool.reset();
    }

protected:
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

    void prepare(spock::FrameState &frame) override
    {
        auto& frameData = static_cast<VideoFrameState&>(frame);

        AVFrame const *videoFrame = m_decoder->currentFrame();
        if (!videoFrame)
        {
            return;
        }
        if (frameData.serial != m_decoder->serial())
        {
            frameData.show(m_foundry, m_sampler, videoFrame, m_decoder->serial());
        }

        // Every submission that uses a frame follows FFmpeg's rules: wait for the frame's semaphore to reach
        // sem_value, signal the next value when done, and record the layout and access the image is left in,
        // which FFmpeg's next barrier starts from.  FFmpeg may have used the image as a decoding reference since
        // the last frame, so this applies even when the frame hasn't changed.
        // The decoder runs on this thread, so nothing else touches the frame before the submission.
        AVVkFrame *vkFrame = frameData.vulkanFrame();

        vk::ImageLayout layout = vk::ImageLayout(vkFrame->layout[0]);
        if (layout != vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            // The semaphore wait makes the decoder's writes visible, and the barrier chains to it through the stage.
            // The image is shared concurrently by the queue families, so there's no ownership transfer.
            vk::ImageMemoryBarrier barrier(
                {},
                vk::AccessFlagBits::eShaderRead,
                layout,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                vk::Image(vkFrame->img[0]),
                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

            frame.commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eFragmentShader,
                {},
                nullptr,
                nullptr,
                barrier);
        }

        frame.extraWaitSemaphores.push_back(
            {vk::Semaphore(vkFrame->sem[0]), vkFrame->sem_value[0], vk::PipelineStageFlagBits::eFragmentShader});
        vkFrame->sem_value[0] += 1;
        frame.extraSignalSemaphores.push_back({vk::Semaphore(vkFrame->sem[0]), vkFrame->sem_value[0]});

        vkFrame->layout[0] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkFrame->access[0] = VK_ACCESS_SHADER_READ_BIT;
    }

    void render(spock::FrameState &frame) override
    {
        auto& frameData = static_cast<VideoFrameState&>(frame);

        // Nothing to draw until the first frame has been decoded.
        if (frameData.serial == 0)
        {
            return;
        }

        // Bind the pipeline and vertex buffers.
        auto& commandBuffer = frame.commandBuffer;

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {frameData.descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Letterbox or pillbox the video.
        float windowAspect = float(m_extents.width) / float(m_extents.height);
        float videoAspect = m_decoder->aspectRatio();
        glm::vec2 scale{1.0f};
        scale.x = (windowAspect > videoAspect) ? (videoAspect / windowAspect) : 1.0f;
        scale.y = (windowAspect < videoAspect) ? (windowAspect / videoAspect) : 1.0f;

        PushConstants pushConstants{m_yuvToRgb, scale, frameData.uvScale};
        spock::pushConstants(commandBuffer, m_pipelineLayout, PUSH_CONSTANT_STAGES, pushConstants);

        commandBuffer.draw(QUAD_VERTEX_COUNT, 1, 0, 0);
    }

private:
    static constexpr vk::ShaderStageFlags PUSH_CONSTANT_STAGES{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment};

    std::shared_ptr<const VulkanVideoDecoder> m_decoder;
    glm::mat4 m_yuvToRgb{bt601LimitedRangeMatrix()};

    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::Sampler m_sampler{nullptr};

    spock::BufferWrapper m_vertexBuffer;
};

class VulkanVideoPlayerApp : public spock::App
{
public:
    explicit VulkanVideoPlayerApp(std::string const &path)
        : VulkanVideoPlayerApp(std::make_shared<VulkanVideoDecoder>(path))
    {
    }

protected:
    spock::FoundryPtr createFoundry() const override
    {
        std::vector<char const*> extensions = m_decoder->requiredDeviceExtensions();
        VulkanVideoDecoder::DeviceFeatures features = VulkanVideoDecoder::requiredDeviceFeatures();

        try
        {
            return std::make_shared<const spock::Foundry>(
                m_instance,
                m_window.createSurface(m_instance),
                vk::QueueFlagBits::eVideoDecodeKHR,
                vk::PhysicalDeviceType::eDiscreteGpu,
                extensions,
                &features.get<vk::PhysicalDeviceFeatures2>());
        }
        catch (...)
        {
            std::cerr << "vulkan_video_player: decoding this video needs a device with a video decode queue and "
                      << extensions.back() << ".\n"
                      << "Mesa's Intel driver only exposes them when run with ANV_DEBUG=video-decode.\n";
            throw;
        }
    }

    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        // FFmpeg decodes on the Foundry's device, so it can only start now that the device exists.
        m_decoder->start(m_instance, m_context.getDispatcher()->vkGetInstanceProcAddr, m_foundry);

        return std::make_unique<VulkanVideoRenderer>(m_foundry, m_window.extents(), m_decoder);
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
    explicit VulkanVideoPlayerApp(std::shared_ptr<VulkanVideoDecoder> decoder)
        : spock::App(
            "Vulkan Video Player",
            decoder->extent().width,
            decoder->extent().height)
        , m_decoder(std::move(decoder))
    {
    }

    std::shared_ptr<VulkanVideoDecoder> m_decoder;
    std::chrono::microseconds m_playbackStart{0};
};

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video file>\n";
        return 1;
    }

    return spock::runApp<VulkanVideoPlayerApp>(std::string(argv[1]));
}

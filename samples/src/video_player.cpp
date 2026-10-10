// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// Plays a video file decoded by FFmpeg, based on the Quad sample.
// Each decoded YUV 4:2:0 frame is written straight into persistently mapped, linearly tiled
// Y, U and V textures, which the fragment shader samples and converts to RGB.
//
// Usage: videoplayer <video file>

#include "video_decoder.h"

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"
#include "vulkan/vulkan.hpp"

#include <vulkan/vulkan_raii.hpp>

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
class VideoFrameState : public spock::FrameState
{
public:
    explicit VideoFrameState(spock::FoundryPtr const &foundry)
        : spock::FrameState(foundry->device(), foundry->commandPool())
    {
    }

    void createResources(
        spock::FoundryPtr const &foundry,
        vk::raii::DescriptorPool const &descriptorPool,
        vk::raii::DescriptorSetLayout const &descriptorSetLayout,
		vk::raii::Sampler const &sampler,
		vk::Extent2D const &lumaExtent)
    {
		if (descriptorSet != nullptr) return;

        vk::Extent2D chromaExtent((lumaExtent.width + 1) / 2, (lumaExtent.height + 1) / 2);

        descriptorSet = std::move(vk::raii::DescriptorSets(foundry->device(), {descriptorPool, *descriptorSetLayout}).front());

		planes.reserve(PLANE_COUNT);
		planes.emplace_back(foundry, lumaExtent);
		planes.emplace_back(foundry, chromaExtent);
		planes.emplace_back(foundry, chromaExtent);

		// Start black rather than with undefined contents.
		std::memset(planes[0].mapped, 0, planes[0].rowPitch * lumaExtent.height);
		std::memset(planes[1].mapped, 128, planes[1].rowPitch * chromaExtent.height);
		std::memset(planes[2].mapped, 128, planes[2].rowPitch * chromaExtent.height);

		std::array<vk::DescriptorImageInfo, PLANE_COUNT> imageInfos;
		std::array<vk::WriteDescriptorSet, PLANE_COUNT> writes;
		for (uint32_t p = 0; p < PLANE_COUNT; ++p)
		{
			imageInfos[p] = vk::DescriptorImageInfo(*sampler, *planes[p].image.imageView(), vk::ImageLayout::eGeneral);
			writes[p] = vk::WriteDescriptorSet(*descriptorSet, p, 0, vk::DescriptorType::eCombinedImageSampler, imageInfos[p]);
		}
		foundry->device().updateDescriptorSets(writes, nullptr);

        foundry->submit(
            [&](vk::CommandBuffer commandBuffer)
            {
				for (VideoPlane const &plane : planes)
				{
					spock::setImageLayout(
						commandBuffer,
						plane.image.image(),
						plane.image.format(),
						vk::ImageLayout::ePreinitialized,
						vk::ImageLayout::eGeneral);
				}
            });
    }

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
        m_createFrameFunc = [](spock::FoundryPtr const& foundry)
        {
            return std::make_unique<VideoFrameState>(foundry);
        };

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

        createPipeline();
    }

	~VideoRenderer() override
	{
        // The frames' descriptor sets are freed back to Renderer::m_descriptorPool, so release them while it still exists.
        m_framePool.reset();
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
        auto& frameData = static_cast<VideoFrameState&>(frame);

		frameData.createResources(m_foundry, m_descriptorPool, m_descriptorSetLayout, m_sampler, m_decoder->extent());
		
        AVFrame const *videoFrame = m_decoder->currentFrame();
        if (videoFrame && frameData.serial != m_decoder->serial())
        {
            for (uint32_t p = 0; p < PLANE_COUNT; ++p)
            {
                frameData.planes[p].upload(videoFrame->data[p], videoFrame->linesize[p]);
            }
            frameData.serial = m_decoder->serial();
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

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// A compute shader generates an image each frame, which a graphics pass then draws on a full-screen quad.
// The compute work runs on the compute queue. When that is a separate queue family, the image's ownership is
// transferred to the graphics queue; otherwise it is the graphics queue and no transfer is needed.

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/file_watcher.hpp"
#include "spock/foundry.hpp"
#include "spock/frame_state.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
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

// How long to wait for a frame's previous compute work before giving up on a hung GPU.
static constexpr std::chrono::nanoseconds COMPUTE_TIMEOUT{std::chrono::seconds(5)};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  vec2 aspect;
} pc;

layout (location = 0) in vec2 pos;

layout (location = 0) out vec2 uv;

void main()
{
  // Letterbox the square image: texture coordinates outside [0, 1] sample the sampler's border color.
  vec2 quadCoord = (pos + 1.0) * 0.5;
  uv = quadCoord * pc.aspect + (vec2(1.0) - pc.aspect) * 0.5;
  gl_Position = vec4(pos, 1.0, 1.0);
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform sampler2D texSampler;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = vec4(texture(texSampler, uv).rgb, 1.0);
}
)";

// nimitz's "Protean clouds" ShaderToy shader, ported from a fragment shader to a compute shader.
// The inputs are in the image's pixels, not the window's.
static const std::string COMPUTE_SHADER_SOURCE = R"(
)";

struct VertexConstants
{
    glm::vec2 aspect;
};

// The member order keeps the C++ layout the same as the GLSL one.
struct ComputeConstants
{
    glm::vec4 iMouse;        // image/buffer xy = current pixel coords (if LMB is down). zw = click pixel
    glm::vec3 iResolution;   // image/buffer The viewport resolution (z is pixel aspect ratio, usually 1.0)
    float iTime;             // image/sound/buffer Current time in seconds
    int iFrame;              // image/buffer Current frame
};
static_assert(offsetof(ComputeConstants, iTime) == 28 && offsetof(ComputeConstants, iFrame) == 32);

static const std::string SHADER_PATH = std::string(SPOCK_DIR) + "/samples/assets/shaders/";
static const std::string COMPUTE_SHADER = "compute_clouds.cs";

// Each frame in flight has its own image, so the compute work for one frame can overlap the drawing of another.
class ComputeQuadFrameState : public spock::FrameState
{
public:
    explicit ComputeQuadFrameState(spock::FoundryPtr const& foundry)
        : spock::FrameState(foundry)
        , computeCommandBuffer(spock::createCommandBuffer(foundry->device(), foundry->computeCommandPool()))
        , computeTimeline(foundry->device().createSemaphore(
            vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo>{
                {}, { vk::SemaphoreType::eTimeline, 0 } }.get()))
    {
    }

    vk::raii::CommandBuffer computeCommandBuffer{nullptr};
    spock::ImageWrapper image;
    vk::raii::DescriptorSet graphicsDescSet{nullptr};
    vk::raii::DescriptorSet computeDescSet{nullptr};

    // Signalled with computeTimelineValue when the frame's compute work completes.
    vk::raii::Semaphore const computeTimeline{nullptr};
    uint64_t computeTimelineValue{0};
};

class ComputeQuadRenderer : public spock::Renderer
{
public:
    ComputeQuadRenderer(
        spock::FoundryPtr const &foundry,
        vk::SurfaceKHR surface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            foundry,
            surface,
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0},
            false)
    {
        // Frames are created by the first renderFrame, after the resources below exist.
        m_createFrameFunc = [this](spock::FoundryPtr const& foundry)
        {
            return createFrameState(foundry);
        };

        createResources();
        createPipelines();
    }

    ~ComputeQuadRenderer() override
    {
        // The frames' descriptor sets are freed back to m_descriptorPool, so release them while it still exists.
        m_framePool.reset();
    }

    void createPipelines(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        // Create the shaders.
        auto vertShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
        auto fragShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);

        std::vector<vk::PipelineShaderStageCreateInfo> graphicsStagesInfo{
            {{}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main"},
        };

        // An opaque full-screen quad needs neither a depth test nor blending.
        m_graphics.pipeline = spock::createGraphicsPipeline(
            m_foundry->device(),
            graphicsStagesInfo,
            m_graphics.layout,
            m_renderPass,
            spock::VertexFormatWrapper<QuadVertex>(),
            vk::PrimitiveTopology::eTriangleStrip,
            vk::CullModeFlagBits::eBack,
            false,
            spock::blendDisabled());

        try
        {
            if ((m_compShader == nullptr) || (shaderStages & vk::ShaderStageFlagBits::eCompute))
			{
				m_compShader = spock::loadShader(m_foundry->device(), vk::ShaderStageFlagBits::eCompute, SHADER_PATH + COMPUTE_SHADER);
            }
        }
        catch (std::exception const& e)
        {
            spock::writeLog(std::string(e.what()));
        }

		if (m_compShader != nullptr)
		{
			vk::PipelineShaderStageCreateInfo computeStageInfo({}, vk::ShaderStageFlagBits::eCompute, *m_compShader, "main");

			m_compute.pipeline = spock::createComputePipeline(
				m_foundry->device(),
				computeStageInfo,
				m_compute.layout);
	    }
	}

protected:
    void createResources()
    {
        m_vertexBuffer = spock::BufferWrapper(
            m_foundry,
            QUAD_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), QUAD_VERTEX_DATA, QUAD_VERTEX_COUNT);

        // Each frame has one set of each kind.
        m_descriptorPool = spock::createDescriptorPool(
            m_foundry->device(),
            { {vk::DescriptorType::eCombinedImageSampler, m_framesInFlight},
              {vk::DescriptorType::eStorageImage, m_framesInFlight} });

        // Graphics pipeline layout.
        m_graphics.descSetLayout = spock::createDescriptorSetLayout(
            m_foundry->device(),
            vk::ShaderStageFlagBits::eFragment,
            {vk::DescriptorType::eCombinedImageSampler});

        vk::PushConstantRange vertexConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(VertexConstants)};

        m_graphics.layout = vk::raii::PipelineLayout(
            m_foundry->device(), {{}, *m_graphics.descSetLayout, vertexConstantRange});

        // Compute pipeline layout.
        m_compute.descSetLayout = spock::createDescriptorSetLayout(
            m_foundry->device(),
            vk::ShaderStageFlagBits::eCompute,
            {vk::DescriptorType::eStorageImage});

        vk::PushConstantRange computeConstantRange{
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(ComputeConstants)};

        m_compute.layout = vk::raii::PipelineLayout(
            m_foundry->device(), {{}, *m_compute.descSetLayout, computeConstantRange});

        // The border color fills the letterbox bars.
        m_sampler = spock::createSampler(
            m_foundry->device(),
            vk::Filter::eLinear,
            vk::SamplerAddressMode::eClampToBorder);
    }

    std::unique_ptr<spock::FrameState> createFrameState(spock::FoundryPtr const& foundry) const
    {
        auto frameState = std::make_unique<ComputeQuadFrameState>(foundry);

        frameState->graphicsDescSet = std::move(vk::raii::DescriptorSets(
            foundry->device(), { m_descriptorPool, *m_graphics.descSetLayout }).front());

        frameState->computeDescSet = std::move(vk::raii::DescriptorSets(
            foundry->device(), { m_descriptorPool, *m_compute.descSetLayout }).front());

        frameState->image = spock::ImageWrapper(
            foundry,
            m_imageFormat,
            m_imageExtent,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eUndefined,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor);

        // The graphics pass samples the image as eShaderReadOnlyOptimal;
        vk::DescriptorImageInfo sampledImageInfo{
            m_sampler,
            frameState->image.imageView(),
            vk::ImageLayout::eShaderReadOnlyOptimal};

        // The compute pass writes it in eGeneral.
        vk::DescriptorImageInfo storageImageInfo{
            nullptr,
            frameState->image.imageView(),
            vk::ImageLayout::eGeneral};

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets{
            {
                *frameState->graphicsDescSet,
                0, 0, 1,
                vk::DescriptorType::eCombinedImageSampler, &sampledImageInfo,
                nullptr, nullptr
            },
            {
                *frameState->computeDescSet,
                0, 0, 1,
                vk::DescriptorType::eStorageImage, &storageImageInfo,
                nullptr, nullptr
            }
        };

        foundry->device().updateDescriptorSets(writeDescriptorSets, nullptr);

        return frameState;
    }

    // Runs the compute pass on the compute queue, then makes the graphics submission wait for it.
    void prepare(spock::FrameState &frame) override
    {
		if (m_compute.pipeline == nullptr) return; // Don't run compute if the shader compilation failed.

        auto& frameData = static_cast<ComputeQuadFrameState&>(frame);
        auto& computeCommandBuffer = frameData.computeCommandBuffer;

        uint32_t const computeFamily = m_foundry->queueFamilies().compute;
        uint32_t const graphicsFamily = m_foundry->queueFamilies().graphics;

        // Safety check: ensure that the compute's previous submission has finished.
        vk::Result waitResult = m_foundry->device().waitSemaphores(
            vk::SemaphoreWaitInfo({}, *frameData.computeTimeline, frameData.computeTimelineValue),
            static_cast<uint64_t>(COMPUTE_TIMEOUT.count()));
        if (waitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Waiting for the compute timeline failed: " + vk::to_string(waitResult));
        }

        computeCommandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        // The FrameStatePool ensures that the previous graphics submission has finished.
        spock::setImageLayout(
            *computeCommandBuffer,
            *frameData.image.image(),
            m_imageFormat,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral);

        computeCommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute.pipeline);
        computeCommandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_compute.layout,
            0,
            {frameData.computeDescSet},
            nullptr);

        // The shader draws into the image, so its resolution and mouse positions are in the image's pixels.
        ComputeConstants computeConstants{
            glm::vec4(0, 0, 0, 0),
            glm::vec3((float)m_imageExtent.width, (float)m_imageExtent.height, 1.0f),
            std::chrono::duration_cast<std::chrono::duration<float>>(m_frameTime).count(),
            (int)m_frameCount};
        spock::pushConstants(computeCommandBuffer, m_compute.layout, vk::ShaderStageFlagBits::eCompute, computeConstants);

        // One invocation per pixel, in 16x16 workgroups to match the shader's local size.
        computeCommandBuffer.dispatch((m_imageExtent.width + 15) / 16, (m_imageExtent.height + 15) / 16, 1);

        // Release the image to the graphics queue and change the image layout to eShaderReadOnlyOptimal.
        vk::ImageSubresourceRange const range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        vk::ImageMemoryBarrier release{
            vk::AccessFlagBits::eShaderWrite, {},
            vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal,
            computeFamily, graphicsFamily,
            *frameData.image.image(), range};
        computeCommandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe,
            {}, nullptr, nullptr, release);

        computeCommandBuffer.end();

        // Signal the next timeline value when the compute work completes.
        uint64_t const signalValue = frameData.computeTimelineValue + 1;
        vk::TimelineSemaphoreSubmitInfo timelineInfo({}, signalValue);
        vk::SubmitInfo submitInfo({}, {}, *computeCommandBuffer, *frameData.computeTimeline, &timelineInfo);
        m_foundry->computeQueue().submit(submitInfo);
        frameData.computeTimelineValue = signalValue;

        // The graphics submission waits for that value before its fragment shader reads the image.
        frameData.extraWaitSemaphores.push_back(
            {*frameData.computeTimeline, signalValue, vk::PipelineStageFlagBits::eFragmentShader});

        if (computeFamily != graphicsFamily)
        {
			// If the queue families are different, the graphics queue must acquire ownership of the image before reading it.
            vk::ImageMemoryBarrier acquire{
                {}, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal,
                computeFamily, graphicsFamily,
                *frameData.image.image(), range};
            frameData.commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
                {}, nullptr, nullptr, acquire);
        }
    }

    void render(spock::FrameState &frame) override
    {
        auto& frameData = static_cast<ComputeQuadFrameState&>(frame);
        auto& commandBuffer = frameData.commandBuffer;

        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            m_graphics.layout,
            0,
            {frameData.graphicsDescSet},
            nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Update the push constants.
        VertexConstants vertexConstants{letterboxScale()};
        spock::pushConstants(commandBuffer, m_graphics.layout, vk::ShaderStageFlagBits::eVertex, vertexConstants);

        commandBuffer.draw(QUAD_VERTEX_COUNT, 1, 0, 0);
    }

private:
    // The vertex shader scales the quad's texture coordinates by this, so the square image keeps its aspect ratio.
    glm::vec2 letterboxScale() const
    {
        glm::vec2 scale{1.0f};
        scale.x = (m_extents.width > m_extents.height) ? (float(m_extents.width) / m_extents.height) : 1.0f;
        scale.y = (m_extents.width < m_extents.height) ? (float(m_extents.height) / m_extents.width) : 1.0f;
        return scale;
    }

    // Converts a window position to the image's pixel coordinates, with y pointing up as ShaderToy expects.
    // It repeats the vertex shader's letterboxing, so positions in the bars fall outside the image.
    glm::vec2 windowToImage(vk::Offset2D const& windowPos) const
    {
        glm::vec2 const quadCoord{float(windowPos.x) / m_extents.width, float(windowPos.y) / m_extents.height};
        glm::vec2 const scale = letterboxScale();
        glm::vec2 const uv = quadCoord * scale + (glm::vec2(1.0f) - scale) * 0.5f;
        return {uv.x * m_imageExtent.width, (1.0f - uv.y) * m_imageExtent.height};
    }

    struct PipelineData
    {
        vk::raii::DescriptorSetLayout descSetLayout{nullptr};
        vk::raii::PipelineLayout layout{nullptr};
        vk::raii::Pipeline pipeline{nullptr};
    };

    // Must be declared before the PipelineData members so it is destroyed after them.
    vk::raii::DescriptorPool m_descriptorPool{nullptr};

    PipelineData m_graphics;
    PipelineData m_compute;

    vk::raii::ShaderModule m_compShader{nullptr};

    spock::BufferWrapper m_vertexBuffer;

    // The format and size of each frame's compute image. Every device supports RGBA8 as a storage image.
    vk::Format const m_imageFormat{vk::Format::eR8G8B8A8Unorm};
    vk::Extent2D const m_imageExtent{512, 512};
    vk::raii::Sampler m_sampler{nullptr};
};

class ComputeQuadApp : public spock::App
{
public:
    ComputeQuadApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Compute Quad",
            windowWidth,
            windowHeight)
        , m_watcher(SHADER_PATH)
    {
    }

protected:
    spock::FoundryPtr createFoundry() const override
    {
        // The Foundry prefers a compute family without graphics, and falls back to the graphics family.
        spock::DeviceRequirements requirements{
            vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute
        };

        return std::make_shared<const spock::Foundry>(
            m_instance,
            m_window.surface(),
            requirements
        );
    }

    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        return std::make_unique<ComputeQuadRenderer>(m_foundry, m_window.surface(), m_window.extents());
    }

    void update() override
    {
        ComputeQuadRenderer* renderer = static_cast<ComputeQuadRenderer*>(m_renderer.get());

        // Check the file watcher for any modified shader files and rebuild the pipeline if necessary.
        auto modifiedFiles = m_watcher.takeModified();
        vk::ShaderStageFlags modifiedShaders{0};
        if (modifiedFiles.contains(COMPUTE_SHADER)) modifiedShaders |= vk::ShaderStageFlagBits::eCompute;

        if (modifiedShaders)
        {
            // If the shader source is changed then rebuild the shaders and recreate the graphics pipeline.
            renderer->waitIdle();
            renderer->createPipelines(modifiedShaders);
        }
    }

private:
    spock::FileWatcher m_watcher;
};

int main()
{
    return spock::runApp<ComputeQuadApp>(500, 500);
}

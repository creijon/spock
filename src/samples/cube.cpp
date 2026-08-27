// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample is a completely self-contained renderer: the cube geometry and both
// shaders are defined inline below, with no external asset or shader files to load.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"

#include "vulkan/vulkan.hpp"

#include <iterator>
#include <utility>
#include <vector>

struct CubeVertex
{
    glm::vec4 pos;
    glm::vec4 rgba;
};

static const CubeVertex CUBE_VERTEX_DATA[] =
{
    // red face
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    // green face
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    // blue face
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    // yellow face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    // magenta face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    // cyan face
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}
};

static constexpr uint32_t CUBE_VERTEX_BUFFER_SIZE{sizeof(CUBE_VERTEX_DATA)};
static constexpr uint32_t CUBE_VERTEX_COUNT{std::size(CUBE_VERTEX_DATA)};
static constexpr uint32_t CUBE_VERTEX_STRIDE{sizeof(CUBE_VERTEX_DATA[0])};
static const std::vector<std::pair<vk::Format, uint32_t>> CUBE_VERTEX_FORMAT{
    {vk::Format::eR32G32B32A32Sfloat, 0},
    {vk::Format::eR32G32B32A32Sfloat, uint32_t(offsetof(CubeVertex, rgba))}
};


static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = inColor;
  gl_Position = pc.mvp * pos;
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 color;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = color;
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
};

class CubeRenderer : public spock::Renderer
{
public:
    CubeRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0})
    {
        // Create the sample cube geometry and the push constants for the model-view-projection matrix.
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, {}, pushConstantRange }));

        m_vertexBuffer = spock::BufferWrapper(m_physicalDevice, m_device, CUBE_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), CUBE_VERTEX_DATA, CUBE_VERTEX_COUNT);

        createGraphicsPipeline();
    }

    void setView(glm::vec3 const& view)
    {
        m_view = view;
    }

protected:
    void createGraphicsPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        // Create the shaders.
        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShaderModule{nullptr};
        vk::raii::ShaderModule fragmentShaderModule{nullptr};
        try
        {
            vertexShaderModule = spock::compileShader(m_device, vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
            fragmentShaderModule = spock::compileShader(m_device, vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        // Finally create the graphics pipeline.
        vk::raii::PipelineCache pipelineCache(m_device, vk::PipelineCacheCreateInfo());
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            pipelineCache,
            vertexShaderModule, nullptr,
            fragmentShaderModule, nullptr,
            CUBE_VERTEX_STRIDE, CUBE_VERTEX_FORMAT,
            vk::FrontFace::eClockwise,
            true,
            m_pipelineLayout,
            m_renderPass);
    }

    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

        // Update the push constants.
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, -1.0f, 0.0f);
        PushConstants pushConstants{spock::viewProjClipMatrix(m_extents, m_view, target, up)};

        vk::ArrayProxyNoTemporaries<const uint8_t> dataSpan{
            sizeof(PushConstants),
            reinterpret_cast<const uint8_t*>(&pushConstants)};

        commandBuffer.pushConstants<uint8_t>(
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            dataSpan);
        
        // Draw all the scene, but for this example it's just a single cube.
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, {0});
        commandBuffer.draw(CUBE_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;

    glm::vec3 m_view{};
};

class CubeApp : public spock::App
{
public:
    CubeApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Cube",
            windowWidth,
            windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<CubeRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();
        
        CubeRenderer* renderer = static_cast<CubeRenderer*>(m_renderer.get());

        renderer->setView(glm::vec3(sinf(angle) * 5.0f, -3.0f, cosf(angle) * 5.0f));
    }
};

int main()
{
    return spock::runApp<CubeApp>(500, 500);
}

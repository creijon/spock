// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample demonstrates instanced rendering with a 16x16 grid of sprites.
// The vertex buffer contains a single quad geometry, and the instance buffer
// contains position and color data for each sprite instance.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

// Vertex structure for a simple quad
struct SpriteVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32G32Sfloat, offsetof(SpriteVertex, pos) },
            { vk::Format::eR32G32Sfloat, offsetof(SpriteVertex, texCoord) }
        };
    }

    glm::vec2 pos;
    glm::vec2 texCoord;
};

// Instance data: position and color
struct SpriteInstance
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32G32Sfloat, offsetof(SpriteInstance, position) },
            { vk::Format::eR32G32B32A32Sfloat, offsetof(SpriteInstance, color) }
        };
    }

    glm::vec2 position;
    glm::vec4 color;
};

// Quad geometry: two triangles forming a 1x1 square centered at origin
static const SpriteVertex QUAD_VERTICES[] = {
    {{-0.5f, -0.5f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f}, {0.0f, 1.0f}},
};

static constexpr uint32_t QUAD_VERTEX_BUFFER_SIZE{sizeof(QUAD_VERTICES)};
static constexpr uint32_t QUAD_VERTEX_COUNT{std::size(QUAD_VERTICES)};

// Create a 16x16 grid of sprite instances
static std::vector<SpriteInstance> createSpriteGrid()
{
    std::vector<SpriteInstance> instances;
    instances.reserve(16 * 16);

    const float gridSpacing = 1.5f;
    const float gridSize = 16.0f * gridSpacing;
    const float startX = -gridSize / 2.0f;
    const float startY = -gridSize / 2.0f;

    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            glm::vec2 position{
                startX + x * gridSpacing,
                startY + y * gridSpacing
            };

            // Create a color gradient based on position
            float r = (x / 15.0f);
            float g = (y / 15.0f);
            float b = 0.5f + 0.5f * std::sin((x + y) * 0.3f);
            glm::vec4 color{r, g, b, 1.0f};

            instances.push_back({position, color});
        }
    }

    return instances;
}

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec2 inInstancePos;
layout (location = 3) in vec4 inColor;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;

void main()
{
    vec2 worldPos = inPos + inInstancePos;
    gl_Position = pc.mvp * vec4(worldPos, 0.0, 1.0);
    outColor = inColor;
    outTexCoord = inTexCoord;
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

void main()
{
    // Simple checkerboard pattern based on UV coords
    float checker = mod(floor(inTexCoord.x * 4.0) + floor(inTexCoord.y * 4.0), 2.0);
    vec3 texColor = mix(vec3(1.0), vec3(0.0), checker);
    
    // Blend texture with instance color
    outColor = vec4(inColor.rgb * texColor, inColor.a);
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
};

class InstancingRenderer : public spock::Renderer
{
public:
    InstancingRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.1f, 0.1f, 0.15f, 1.0},
            {1.0f, 0})
    {
        // Create the sprite geometry and push constants for the MVP matrix
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, {}, pushConstantRange}));

        // Create vertex buffer for quad geometry
        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            QUAD_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), QUAD_VERTICES, QUAD_VERTEX_COUNT);

        // Create instance buffer
        auto instances = createSpriteGrid();
        m_instanceCount = instances.size();
        m_instanceBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            instances.size() * sizeof(SpriteInstance),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_instanceBuffer.deviceMemory(), instances.data(), instances.size());

        createGraphicsPipeline();
    }

    void setView(glm::vec3 const& view)
    {
        m_view = view;
    }

protected:
    void createGraphicsPipeline()
    {
        // Create the shaders
        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShader{nullptr};
        vk::raii::ShaderModule fragmentShader{nullptr};
        try
        {
            vertexShader = spock::compileShader(m_device, vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
            fragmentShader = spock::compileShader(m_device, vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        const vk::PipelineShaderStageCreateFlags shaderStageCreateFlags{};
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
            {shaderStageCreateFlags, vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"},
            {shaderStageCreateFlags, vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"},
        };

        // Create vertex format combining vertex and instance attributes
        spock::VertexFormat vertexFormat;
        vertexFormat.addAttributes<SpriteVertex>(0, vk::VertexInputRate::eVertex);
        vertexFormat.addAttributes<SpriteInstance>(1, vk::VertexInputRate::eInstance);

        // Create the graphics pipeline
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            shaderStagesInfo,
            m_pipelineLayout,
            m_renderPass,
            vertexFormat,
            vk::PrimitiveTopology::eTriangleList,
            vk::CullModeFlagBits::eNone);
    }

    void render(vk::raii::CommandBuffer const& commandBuffer, std::chrono::microseconds time) override
    {
        // Bind the pipeline
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

        // Update push constants
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, 1.0f, 0.0f);
        PushConstants pushConstants{spock::viewProjClipMatrix(m_extents, m_view, target, up)};
        commandBuffer.pushConstants(
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants),
            reinterpret_cast<const uint8_t*>(&pushConstants));

        // Bind vertex and instance buffers
        commandBuffer.bindVertexBuffers(
            0,
            {m_vertexBuffer.buffer(), m_instanceBuffer.buffer()},
            {0, 0});

        // Draw instanced: all vertices for each instance
        commandBuffer.draw(QUAD_VERTEX_COUNT, m_instanceCount, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_instanceBuffer;
    uint32_t m_instanceCount = 0;

    glm::vec3 m_view{};
};

class InstancingApp : public spock::App
{
public:
    InstancingApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Instancing",
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
        return std::make_unique<InstancingRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();

        InstancingRenderer* renderer = static_cast<InstancingRenderer*>(m_renderer.get());

        // Orbit around the grid
        float distance = 30.0f;
        renderer->setView(glm::vec3(
            std::sin(angle * 0.5f) * distance,
            15.0f,
            std::cos(angle * 0.5f) * distance));
    }
};

int main()
{
    return spock::runApp<InstancingApp>(800, 800);
}

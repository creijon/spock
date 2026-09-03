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
    {{ 0.5f,  0.5f}, {1.0f, 1.0f}}
};

static constexpr uint32_t QUAD_VERTEX_BUFFER_SIZE{sizeof(QUAD_VERTICES)};
static constexpr uint32_t QUAD_VERTEX_COUNT{std::size(QUAD_VERTICES)};

// Create a 16x16 grid of sprite instances
static std::vector<SpriteInstance> createSpriteGrid()
{
    static const uint32_t dim = 16;
    std::vector<SpriteInstance> instances;
    instances.reserve(dim * dim);

    const float gridSpacing = 1.5f;
    const float gridSize = dim * gridSpacing;
    const float startX = -gridSize * 0.5f;
    const float startY = -gridSize * 0.5f;

    for (int y = 0; y < dim; ++y)
    {
        for (int x = 0; x < dim; ++x)
        {
            glm::vec2 position{
                startX + x * gridSpacing,
                startY + y * gridSpacing
            };

            // Create a color gradient based on position
            float r = (float(x) / (dim - 1));
            float g = (float(y) / (dim - 1));
            float b = 0.5f + 0.5f * std::sin((x + y) * 5.0f / (dim - 1));
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
    mat4 view;
    mat4 proj;
    mat4 invView;
    vec2 viewport;
} pc;

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec2 inInstancePos;
layout (location = 3) in vec4 inColor;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;

void main()
{
    mat4 clip = mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
    );
    mat4 mvp = clip * pc.proj * pc.view;

    vec3 cameraRight = vec3(pc.invView[0][0], pc.invView[0][1], pc.invView[0][2]);
    vec3 cameraUp = vec3(pc.invView[1][0], pc.invView[1][1], pc.invView[1][2]);
    vec3 worldPos = vec3(inInstancePos, 0.0) + cameraRight * inPos.x + cameraUp * inPos.y;
    gl_Position = mvp * vec4(worldPos, 1.0);
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
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invView;
    glm::vec2 viewport;
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
            {0.2f, 0.2f, 0.35f, 1.0},
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

    void update(spock::OrbitCamera const& camera, vk::Extent2D const& viewExtents)
    {
        m_cameraConstants.view = camera.view();
        m_cameraConstants.proj = camera.projection(viewExtents);
        m_cameraConstants.invView = glm::inverse(camera.view());
        m_cameraConstants.viewport = { viewExtents.width, viewExtents.height };
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
            vk::PrimitiveTopology::eTriangleStrip,
            vk::CullModeFlagBits::eNone);
    }

    void render(vk::raii::CommandBuffer const& commandBuffer, std::chrono::microseconds time) override
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, m_cameraConstants);
        commandBuffer.bindVertexBuffers(
            0,
            {m_vertexBuffer.buffer(), m_instanceBuffer.buffer()},
            {0, 0});

        commandBuffer.draw(QUAD_VERTEX_COUNT, m_instanceCount, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_instanceBuffer;
    uint32_t m_instanceCount = 0;

    PushConstants m_cameraConstants{};
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
        m_camera.setDistance(30.0f);
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

        vk::Offset2D cursor = m_window.cursorPosition();
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            static const float sensitivity = 0.005f;
            m_camera.update(glm::vec2(
                static_cast<float>(m_previousCursor.x - cursor.x) * sensitivity,
                static_cast<float>(cursor.y - m_previousCursor.y) * sensitivity));
        }
        m_previousCursor = cursor;
        renderer->update(m_camera, m_window.extents());
    }

    vk::Offset2D m_previousCursor{};
    spock::OrbitCamera m_camera{ glm::vec3(0.0f), 5.0f };
};

int main()
{
    return spock::runApp<InstancingApp>(800, 800);
}

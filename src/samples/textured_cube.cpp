// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample extends the basic cube sample by loading a PNG texture from disk with lodepng
// and mapping it onto the faces of the cube via a combined image sampler.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spock/loader.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct CubeVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32G32B32A32Sfloat, offsetof(CubeVertex, pos) },
            { vk::Format::eR32G32Sfloat, offsetof(CubeVertex, uv) },
            { vk::Format::eR32G32B32Sfloat, offsetof(CubeVertex, normal) }
        };
    }

    glm::vec4 pos;
    glm::vec2 uv;
    glm::vec3 normal;
};

static const glm::vec3 xPos{ 1.0f,  0.0f,  0.0f};
static const glm::vec3 xNeg{-1.0f,  0.0f,  0.0f};
static const glm::vec3 yPos{ 0.0f,  1.0f,  0.0f};
static const glm::vec3 yNeg{ 0.0f, -1.0f,  0.0f};
static const glm::vec3 zPos{ 0.0f,  0.0f,  1.0f};
static const glm::vec3 zNeg{ 0.0f,  0.0f, -1.0f};

static const CubeVertex CUBE_VERTEX_DATA[] =
{
    // +Z face
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, zPos},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, zPos},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, zPos},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, zPos},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, zPos},
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, zPos},
    // -Z face
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, zNeg},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, zNeg},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, zNeg},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, zNeg},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, zNeg},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, zNeg},
    // -X face
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, xNeg},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, xNeg},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, xNeg},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, xNeg},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, xNeg},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, xNeg},
    // +X face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, xPos},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, xPos},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, xPos},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, xPos},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, xPos},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, xPos},
    // +Y face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, yPos},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, yPos},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, yPos},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, yPos},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, yPos},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, yPos},
    // -Y face
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, yNeg},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, yNeg},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, yNeg},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, yNeg},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, yNeg},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, yNeg}
};

static constexpr uint32_t CUBE_VERTEX_BUFFER_SIZE{sizeof(CUBE_VERTEX_DATA)};
static constexpr uint32_t CUBE_VERTEX_COUNT{std::size(CUBE_VERTEX_DATA)};

static const std::string TEXTURE_PATH = std::string(SPOCK_DIR) + "/assets/textures/uvmapping.png";

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 itModel;
} pc;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec2 outUv;
layout (location = 1) out vec3 outNormal;

void main()
{
  outUv = uv;
  outNormal = (pc.itModel * vec4(normal, 0.0)).xyz;
  gl_Position = pc.mvp * pos;
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform sampler2D texSampler;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec4 outColor;

void main()
{
  vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
  vec3 lightDif = vec3(1.0);
  vec3 lightAmb = vec3(0.2);
  vec3 litColor = lightAmb + lightDif * max(dot(normalize(normal), lightDir), 0.0);
  outColor = vec4(texture(texSampler, uv).rgb * litColor, 1.0);
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
    glm::mat4x4 itModel;
};

class TexturedCubeRenderer : public spock::Renderer
{
public:
    TexturedCubeRenderer(
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
        // Create the cube geometry, the texture, and the push constants for the
        // model-view-projection matrix.
        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            CUBE_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), CUBE_VERTEX_DATA, CUBE_VERTEX_COUNT);

        m_texture = spock::Loader::texture(*this, m_presenter->graphicsQueue(), TEXTURE_PATH);

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            vk::ShaderStageFlagBits::eFragment,
            {vk::DescriptorType::eCombinedImageSampler});

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, *m_descriptorSetLayout, pushConstantRange }));

        m_descriptorPool = spock::createDescriptorPool(
            m_device,
            { {vk::DescriptorType::eCombinedImageSampler, 1} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, { m_descriptorPool, *m_descriptorSetLayout }).front());

        spock::updateDescriptorSets(m_device, m_descriptorSet, {}, {m_texture});

        createPipeline();
    }

    void setView(glm::vec3 const& view)
    {
        m_view = view;
    }

protected:
    void createPipeline()
    {
        // Create the shaders.
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

        // Finally create the graphics pipeline.
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            shaderStagesInfo,
            m_pipelineLayout,
            m_renderPass.renderPass(),
            spock::VertexFormatWrapper<CubeVertex>());
    }

    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // Bind the pipeline, texture descriptor set, and vertex buffer.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Update the push constants.
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, 1.0f, 0.0f);
        static const glm::mat4x4 model(1.0f);
        static const glm::mat4x4 invTransModel = glm::transpose(glm::inverse(model));
        glm::mat4x4 modelViewProj = spock::viewProjMatrix(m_extents, m_view, target, up);
        PushConstants pushConstants{modelViewProj, invTransModel};
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, pushConstants);

        // Draw all the scene, but for this example it's just a single cube.
        commandBuffer.draw(CUBE_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::TextureWrapper m_texture;

    glm::vec3 m_view{};
};

class TexturedCubeApp : public spock::App
{
public:
    TexturedCubeApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Textured Cube",
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
        return std::make_unique<TexturedCubeRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();

        TexturedCubeRenderer* renderer = static_cast<TexturedCubeRenderer*>(m_renderer.get());

        const float radius = 5.0f;
        renderer->setView(glm::vec3(sinf(angle) * radius, 3.0f, cosf(angle) * radius));
    }
};

int main()
{
    return spock::runApp<TexturedCubeApp>(500, 500);
}

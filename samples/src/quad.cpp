// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// An even more simple example than the cube - just a single quad on the screen.
// Useful as a basis for 2D rendering samples.

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/helpers.hpp"
#include "spock/loader.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
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

static const std::string TEXTURE_PATH = std::string(SPOCK_DIR) + "/samples/assets/textures/uvmapping.png";

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  vec2 aspect;
} pc;

layout (location = 0) in vec2 pos;

layout (location = 0) out vec2 fragCoord;

void main()
{
  vec2 uv = (pos + 1.0) * 0.5;
  fragCoord = uv * pc.aspect + (vec2(1.0) - pc.aspect) * 0.5;
  gl_Position = vec4(pos, 1.0, 1.0);
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

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

struct PushConstants
{
    glm::vec2 aspect;
};

class QuadRenderer : public spock::Renderer
{
public:
    QuadRenderer(
        spock::FoundryPtr const &foundry,
        vk::Extent2D const& extents)
        : spock::Renderer(
            foundry,
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0})
    {
        m_vertexBuffer = spock::BufferWrapper(
            foundry,
            QUAD_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), QUAD_VERTEX_DATA, QUAD_VERTEX_COUNT);

        m_texture = spock::Loader::texture(
            foundry,
            TEXTURE_PATH,
            spock::createSampler(foundry->device(), vk::Filter::eLinear, vk::SamplerAddressMode::eClampToBorder));

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            foundry->device(),
            vk::ShaderStageFlagBits::eFragment,
            {vk::DescriptorType::eCombinedImageSampler});

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(foundry->device(), {{}, *m_descriptorSetLayout, pushConstantRange}));

        m_descriptorPool = spock::createDescriptorPool(
            foundry->device(),
            { {vk::DescriptorType::eCombinedImageSampler, 1} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(foundry->device(), { m_descriptorPool, *m_descriptorSetLayout }).front());

        spock::updateDescriptorSets(foundry->device(), m_descriptorSet, {}, {m_texture});

        createPipeline();
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
            vk::PrimitiveTopology::eTriangleStrip);
    }

    void render(spock::FrameState &frame) override
    {
        auto& commandBuffer = frame.commandBuffer;
        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Update the push constants.
        glm::vec2 aspect{1.0f};
        aspect.x = (m_extents.width > m_extents.height) ? (float(m_extents.width) / m_extents.height) : 1.0f;
        aspect.y = (m_extents.width < m_extents.height) ? (float(m_extents.height) / m_extents.width) : 1.0f;
        PushConstants pushConstants{aspect};
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, pushConstants);

        commandBuffer.draw(QUAD_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::TextureWrapper m_texture;
};

class QuadApp : public spock::App
{
public:
    QuadApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Quad",
            windowWidth,
            windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        return std::make_unique<QuadRenderer>(m_foundry, m_window.extents());
    }

    void update() override
    {
    }
};

int main()
{
    return spock::runApp<QuadApp>(500, 500);
}

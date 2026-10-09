// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/debug_lines.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/shaders.hpp"

#include <stdexcept>

namespace spock
{
    namespace
    {
        const char* const vertShaderSource = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    gl_Position = pc.viewProjection * vec4(inPosition, 1.0);
    outColor = inColor;
}
)";

        const char* const fragShaderSource = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = inColor;
}
)";

        struct PushConstants
        {
            glm::mat4 viewProjection;
        };
    }

    DebugLines::DebugLines(
        FoundryPtr const &foundry,
        vk::raii::RenderPass const& renderPass,
        size_t maxLineCount)
        : m_maxLineCount(maxLineCount)
    {
        m_vertices.reserve(maxLineCount * 2);

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};
        m_pipelineLayout = vk::raii::PipelineLayout(
            foundry->device(),
            vk::PipelineLayoutCreateInfo({}, {}, pushConstantRange));

        vk::raii::ShaderModule vertShader = compileShader(
            foundry->device(), vk::ShaderStageFlagBits::eVertex, vertShaderSource);
        vk::raii::ShaderModule fragShader = compileShader(
            foundry->device(), vk::ShaderStageFlagBits::eFragment, fragShaderSource);

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{
            {{}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main"}};

        m_pipeline = createGraphicsPipeline(
            foundry->device(),
            shaderStages,
            m_pipelineLayout,
            renderPass,
            VertexFormatWrapper<Vertex>(),
            vk::PrimitiveTopology::eLineList,
            vk::CullModeFlagBits::eNone,
            true);
    }

    void DebugLines::clear()
    {
        m_vertices.clear();
    }

    void DebugLines::addLine(
        glm::vec3 const& start,
        glm::vec3 const& end,
        glm::vec4 const& color)
    {
        if (m_vertices.size() / 2 >= m_maxLineCount)
        {
            throw std::length_error("DebugLines capacity exceeded");
        }

        m_vertices.push_back({start, color});
        m_vertices.push_back({end, color});
    }

    void DebugLines::draw(
        vk::raii::CommandBuffer const& commandBuffer,
        BufferWrapper const& frameData,
        glm::mat4 const& viewProjection)
    {
        if (m_vertices.empty())
        {
            return;
        }

        if (m_vertices.size() * sizeof(Vertex) > frameData.size())
        {
            throw std::length_error("DebugLines frame buffer capacity exceeded");
        }
        // The caller has waited for this frame's fence; other frames keep their own copies.
        frameData.upload(m_vertices);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
        pushConstants(
            commandBuffer,
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            PushConstants{viewProjection});
        commandBuffer.bindVertexBuffers(0, {*frameData.buffer()}, {0});
        commandBuffer.draw(static_cast<uint32_t>(m_vertices.size()), 1, 0, 0);
    }
} // namespace spock

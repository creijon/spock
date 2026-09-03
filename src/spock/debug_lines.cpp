// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "debug_lines.hpp"

#include "creators.hpp"
#include "shaders.hpp"

#include <glslang/Public/ShaderLang.h>

#include <stdexcept>
#include <utility>

namespace spock
{
    namespace
    {
        const char* const vertexShaderSource = R"(
#version 400

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

        const char* const fragmentShaderSource = R"(
#version 400

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
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& device,
        vk::raii::RenderPass const& renderPass,
        size_t maxLineCount)
        : m_vertexBuffer(
            physicalDevice,
            device,
            maxLineCount * 2 * sizeof(Vertex),
            vk::BufferUsageFlagBits::eVertexBuffer)
    {
        m_vertices.reserve(maxLineCount * 2);

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};
        m_pipelineLayout = vk::raii::PipelineLayout(
            device,
            vk::PipelineLayoutCreateInfo({}, {}, pushConstantRange));

        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShader{nullptr};
        vk::raii::ShaderModule fragmentShader{nullptr};
        try
        {
            vertexShader = compileShader(
                device, vk::ShaderStageFlagBits::eVertex, vertexShaderSource);
            fragmentShader = compileShader(
                device, vk::ShaderStageFlagBits::eFragment, fragmentShaderSource);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{
            {{}, vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"}};

        m_pipeline = createGraphicsPipeline(
            device,
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
        m_verticesDirty = true;
    }

    void DebugLines::addLine(
        glm::vec3 const& start,
        glm::vec3 const& end,
        glm::vec4 const& color)
    {
        if (m_vertices.size() + 2 > m_vertices.capacity())
        {
            throw std::length_error("DebugLines capacity exceeded");
        }

        m_vertices.push_back({start, color});
        m_vertices.push_back({end, color});
        m_verticesDirty = true;
    }

    void DebugLines::draw(
        vk::raii::CommandBuffer const& commandBuffer,
        glm::mat4 const& viewProjection)
    {
        if (m_vertices.empty())
        {
            return;
        }

        if (m_verticesDirty)
        {
            m_vertexBuffer.upload(m_vertices);
            m_verticesDirty = false;
        }
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
        pushConstants(
            commandBuffer,
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            PushConstants{viewProjection});
        commandBuffer.bindVertexBuffers(0, {*m_vertexBuffer.buffer()}, {0});
        commandBuffer.draw(static_cast<uint32_t>(m_vertices.size()), 1, 0, 0);
    }
} // namespace spock

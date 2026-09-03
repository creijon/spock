// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "math.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace spock
{
    class DebugLines
    {
    public:
        struct Vertex
        {
            static VertexFormat::Attributes attributes()
            {
                return {
                    {vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)},
                    {vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color)}};
            }

            glm::vec3 position;
            glm::vec4 color;
        };

        DebugLines(
            vk::raii::PhysicalDevice const& physicalDevice,
            vk::raii::Device const& device,
            vk::raii::RenderPass const& renderPass,
            size_t maxLineCount = 1024);

        void clear();
        void addLine(glm::vec3 const& start, glm::vec3 const& end, glm::vec4 const& color);

        void draw(
            vk::raii::CommandBuffer const& commandBuffer,
            glm::mat4 const& viewProjection);

        size_t lineCount() const
        {
            return m_vertices.size() / 2;
        }

    private:
        vk::raii::PipelineLayout m_pipelineLayout{nullptr};
        vk::raii::Pipeline m_pipeline{nullptr};
        BufferWrapper m_vertexBuffer;
        std::vector<Vertex> m_vertices;
        bool m_verticesDirty{false};
    };
} // namespace spock

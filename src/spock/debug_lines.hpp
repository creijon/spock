// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "math.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace spock
{
    class Foundry;

    // Simple debug line renderer.
    // Geometry and pipeline are shared; the caller supplies frame-owned upload resources.
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

        struct FrameData
        {
            explicit FrameData(std::shared_ptr<const Foundry> const &foundry, size_t maxLineCount = 1024);
            BufferWrapper vertexBuffer;
        };

        DebugLines(
            std::shared_ptr<const Foundry> const &foundry,
            vk::raii::RenderPass const& renderPass,
            size_t maxLineCount = 1024);

        void clear();
        void addLine(glm::vec3 const& start, glm::vec3 const& end, glm::vec4 const& color);

        // FrameData must belong to a frame whose fence has completed.
        void draw(
            vk::raii::CommandBuffer const& commandBuffer,
            FrameData &frameData,
            glm::mat4 const& viewProjection);

        size_t lineCount() const
        {
            return m_vertices.size() / 2;
        }

    private:
        vk::raii::PipelineLayout m_pipelineLayout{nullptr};
        vk::raii::Pipeline m_pipeline{nullptr};
        std::vector<Vertex> m_vertices;
        size_t m_maxLineCount;
    };
} // namespace spock

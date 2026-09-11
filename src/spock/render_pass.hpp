// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace spock
{
    // Owns a render pass, the framebuffers built against it (one per swapchain
    // image), and the depth buffer that backs them when depth is enabled.
    // Also drives beginning/ending the render pass for a frame.
    class RenderPass
    {
    public:
        RenderPass(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            std::vector<vk::raii::ImageView> const& imageViews,
            vk::Format colorFormat,
            vk::Extent2D const &extent,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true);
        RenderPass() = default;
        RenderPass(const RenderPass &) = delete;
        RenderPass(RenderPass &&other) noexcept;
        RenderPass const& operator=(RenderPass &&other);

        vk::raii::RenderPass const& renderPass() const
        {
            return m_renderPass;
        }

        void begin(
            vk::raii::CommandBuffer const &commandBuffer,
            uint32_t frameBufferIndex,
            vk::Extent2D const &extent) const;
        void end(vk::raii::CommandBuffer const &commandBuffer) const;

    private:
        vk::raii::RenderPass m_renderPass{nullptr};
        std::vector<vk::raii::Framebuffer> m_frameBuffers;
        DepthBufferWrapper m_depthBuffer;
        vk::ClearColorValue m_clearColor{};
        vk::ClearDepthStencilValue m_clearDepthStencil{};
    };
} // namespace spock

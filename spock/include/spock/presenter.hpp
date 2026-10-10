// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <vector>

namespace spock
{
    class FrameState;

    class Presenter
    {
    public:
        // When replacing an existing presenter, e.g. on resize, pass it as `oldPresenter` so its swapchain
        // is handed over to the new one.
        Presenter(
            FoundryPtr const &foundry,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t framesInFlight,
            Presenter const *oldPresenter = nullptr);
        Presenter() = default;
        Presenter(const Presenter &) = delete;
        Presenter(Presenter && other) noexcept;
        // The GPU must have finished using this Presenter's resources before move assignment.
        Presenter const& operator=(Presenter && other);

        std::vector<vk::raii::ImageView> const& imageViews() const
        {
            return m_imageViews;
        }

        vk::Format colorFormat() const
        {
            return m_colorFormat;
        }

        // The extent the swapchain was actually created with. This can differ from the extent
        // requested in the constructor when the surface constrains it, so size render targets from this.
        vk::Extent2D extent() const
        {
            return m_extent;
        }

        // Call after creating the render pass and optional depth attachment.
        // The GPU must be idle before replacing or clearing existing framebuffers.
        void createFramebuffers(
            vk::raii::RenderPass const &renderPass,
            vk::raii::ImageView const *depthImageView = nullptr);
        void clearFramebuffers();

        vk::raii::Framebuffer const &framebuffer(uint32_t imageIndex) const
        {
            return m_frameBuffers.at(imageIndex);
        }

        vk::Result acquireFrame(FrameState &frame);
        vk::Result presentFrame(FrameState &frame);

    private:
        // The Foundry has to be the first member since it holds the lifetime of the device and this
        // must be maintained until after the swapchain is destroyed.
        FoundryPtr m_foundry;
        vk::raii::SwapchainKHR m_swapchain{nullptr};
        vk::Format m_colorFormat{};
        vk::Extent2D m_extent{};
    
        std::vector<vk::Image> m_images;
        std::vector<vk::raii::ImageView> m_imageViews;
        // Destroy framebuffers before the swapchain image views they reference.
        std::vector<vk::raii::Framebuffer> m_frameBuffers;

        std::vector<vk::raii::Semaphore> m_renderSemaphores;
    };
} // namespace spock

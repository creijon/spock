// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "foundry.hpp"
#include "types.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <vector>

namespace spock
{
    class FrameState;

    class Presenter
    {
    public:
        // The surface must outlive the Presenter.
        // When replacing an existing presenter for the same surface, e.g. on resize, pass it as `oldPresenter` so
        // its swapchain is handed over to the new one.  A presenter for a different surface can't be handed over.
        // The framebuffers are built for renderPass, with a depth buffer unless depthFormat is eUndefined.
        // The GPU must have finished with the framebuffers and depth buffer before the Presenter is destroyed.
        Presenter(
            FoundryPtr const &foundry,
            vk::SurfaceKHR surface,
            vk::Extent2D const &extent,
            vk::raii::RenderPass const &renderPass,
            vk::Format depthFormat,
            vk::ImageUsageFlags usage,
            uint32_t framesInFlight,
            Presenter const *oldPresenter = nullptr,
            SwapchainOptions const& options = {});
        Presenter(const Presenter &) = delete;
        Presenter &operator=(const Presenter &) = delete;

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

        // The renderer must rotate its output by this.
        vk::SurfaceTransformFlagBitsKHR preTransform() const
        {
            return m_preTransform;
        }

        vk::raii::Framebuffer const &framebuffer(uint32_t imageIndex) const
        {
            return m_frameBuffers.at(imageIndex);
        }

        // The present mode and composite alpha chosen from the SwapchainOptions.
        vk::PresentModeKHR presentMode() const
        {
            return m_presentMode;
        }
        vk::CompositeAlphaFlagBitsKHR compositeAlpha() const
        {
            return m_compositeAlpha;
        }

        vk::Result acquireFrame(FrameState& frame);
        vk::Result presentFrame(FrameState &frame);

    private:
        // The Foundry has to be the first member since it holds the lifetime of the device and this
        // must be maintained until after the swapchain is destroyed.
        FoundryPtr m_foundry;

        std::vector<vk::raii::Semaphore> m_renderSemaphores;

		// Order is important for swapchain and imageview destruction.
        vk::raii::SwapchainKHR m_swapchain{nullptr};
        std::vector<vk::Image> m_images;
        std::vector<vk::raii::ImageView> m_imageViews;
		ImageWrapper m_depthBuffer;
        std::vector<vk::raii::Framebuffer> m_frameBuffers;

		vk::Format m_colorFormat{};
        vk::Extent2D m_extent{};
        vk::SurfaceTransformFlagBitsKHR m_preTransform{vk::SurfaceTransformFlagBitsKHR::eIdentity};
        vk::PresentModeKHR m_presentMode{vk::PresentModeKHR::eFifo};
        vk::CompositeAlphaFlagBitsKHR m_compositeAlpha{vk::CompositeAlphaFlagBitsKHR::eOpaque};
    
    };
} // namespace spock

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace spock
{
    class Foundry;

    class Presenter
    {
    public:
        // When replacing an existing presenter, e.g. on resize, pass it as `oldPresenter` so its swapchain
        // is handed over to the new one. The old presenter must outlive this constructor, and the GPU
        // must have finished with it; its swapchain is retired and can no longer acquire images.
        Presenter(
            std::shared_ptr<const Foundry> const &foundry,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t framesInFlight,
            Presenter const *oldPresenter = nullptr);
        Presenter() = default;
        Presenter(const Presenter &) = delete;
        Presenter(Presenter && other) noexcept;
        Presenter const& operator=(Presenter && other);

        std::vector<vk::raii::ImageView> const& imageViews() const
        {
            return m_imageViews;
        }

        uint32_t imageIndex() const
        {
            return m_imageIndex;
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

        vk::Result acquireFrame(uint32_t frameIndex);
        vk::Result submitCommands(vk::raii::CommandBuffer const& commandBuffer, uint32_t frameIndex);
        vk::Result presentFrame(uint32_t frameIndex);

    private:
        // The Foundry has to be the first member since it holds the lifetime of the device and this
        // must be maintained until after the swapchain is destroyed.s
        std::shared_ptr<const Foundry> m_foundry;
        vk::raii::SwapchainKHR m_swapchain{nullptr};
        vk::Format m_colorFormat{};
        vk::Extent2D m_extent{};
    
        std::vector<vk::Image> m_images;
        std::vector<vk::raii::ImageView> m_imageViews;
        uint32_t m_imageIndex{0};

        // Sized to framesInFlight, and indexed by the frame index the caller
        // passes to acquireFrame/submitCommands/presentFrame -- NOT by the
        // swapchain image index, since the two can differ and must not be
        // conflated.
        std::vector<vk::raii::Semaphore> m_imageSemaphores;
        std::vector<vk::raii::Semaphore> m_renderSemaphores;
        std::vector<vk::raii::Fence> m_frameFences;
    };
} // namespace spock

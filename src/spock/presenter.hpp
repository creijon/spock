// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <vector>

namespace spock
{
    class Presenter
    {
    public:
        Presenter(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::raii::SurfaceKHR const &surface,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            QueueIndices queueIndices,
            uint32_t framesInFlight);
        Presenter() = default;
        Presenter(const Presenter &) = delete;
        Presenter(Presenter && other) noexcept;
        Presenter const& operator=(Presenter && other);

        void initialise(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::raii::SurfaceKHR const &surface,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            QueueIndices queueIndices,
            uint32_t framesInFlight);

        std::vector<vk::raii::ImageView> const& imageViews() const
        {
            return m_imageViews;
        }

        uint32_t imageIndex() const
        {
            return m_imageIndex;
        }

        bool isValid() const
        {
            return m_valid;
        }

        void acquireFrame(vk::raii::Device const &device, uint32_t frameIndex);
        void submitCommands(vk::raii::CommandBuffer const& commandBuffer, uint32_t frameIndex);
        vk::Result presentFrame(uint32_t frameIndex);

    private:
        vk::Format m_colorFormat;
    
        vk::raii::SwapchainKHR m_swapchain{nullptr};
        vk::raii::Queue m_graphicsQueue{nullptr};
        vk::raii::Queue m_presentQueue{nullptr};
    
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

        bool m_valid{false};
    };
} // namespace spock

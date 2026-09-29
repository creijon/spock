// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

namespace spock
{
    // The result of Foundry::createSwapchain.
    // The extent is the one the swapchain was actually created with, which may differ from srequested.
    struct SwapchainInfo
    {
        vk::raii::SwapchainKHR swapchain{nullptr};
        vk::Format colorFormat{vk::Format::eUndefined};
        vk::Extent2D extent{};
    };

    // Responsible for managing the lifetime of the devices, queues and command pools, and keeping track of the queue families.
    // Helps to simplify the calling convention for the wrappers, presenter, renderer etc.  But doesn't own them.
    // It lives in the app, next to the renderer.
    // The danger is that it becomes a bit of a god class, so I've kept the functionality to a minimum.
    // Initially just pass it around and use the accessors, then move functions in where they make sense.
    class Foundry final
    {
    public:
        Foundry(
            vk::raii::Instance const &instance,
            vk::raii::SurfaceKHR windowSurface,
            std::vector<char const*> const &extensions = {},
            void const *features = nullptr);

        ~Foundry();

        vk::raii::PhysicalDevice const &physicalDevice() const { return m_physicalDevice; }
        vk::raii::SurfaceKHR const &surface() const { return m_surface; }
        vk::raii::Device const &device() const { return m_device; }

        vk::raii::CommandPool const &commandPool() const { return m_commandPool; }
        vk::raii::CommandPool const &computeCommandPool() const { return m_computeCommandPool; }

        vk::raii::Queue const &graphicsQueue() const { return m_graphicsQueue; }
        vk::raii::Queue const &computeQueue() const { return m_computeQueue; }
        vk::raii::Queue const &presentQueue() const { return m_presentQueue; }

        // Create a swapchain for the surface.
        // `desiredImageCount` is clamped to what the surface supports; so the caller should use
        // the swapchain's actual image count.
        SwapchainInfo createSwapchain(
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t desiredImageCount) const;

        void waitIdle() const;

        template <typename Func>
        void submit(Func const &func) const
        {
            oneTimeSubmit(m_device, m_commandPool, m_graphicsQueue, func);
        }

        // Records a single-use command buffer via `func` and submits it to the compute queue.
        template <typename Func>
        void submitCompute(Func const &func) const
        {
            oneTimeSubmit(m_device, m_computeCommandPool, m_computeQueue, func);
        }

    private:
        std::vector<vk::DeviceQueueCreateInfo> uniqueCreateInfos() const;

        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::SurfaceKHR m_surface{nullptr};
        vk::raii::Device m_device{nullptr};

        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::CommandPool m_computeCommandPool{nullptr};

        vk::raii::Queue m_graphicsQueue{nullptr};
        vk::raii::Queue m_presentQueue{nullptr};
        vk::raii::Queue m_computeQueue{nullptr};

        uint32_t m_graphicsFamily{0};
        uint32_t m_presentFamily{0};
        uint32_t m_computeFamily{0};
        uint32_t m_transferFamily{0};
    };
}

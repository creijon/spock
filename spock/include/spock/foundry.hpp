// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"
#include "types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

namespace spock
{
    // The result of Foundry::createSwapchain.
    // The extent is the one the swapchain was actually created with, which may differ from requested.
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
    class Foundry final
    {
    public:
        // For the moment, device with graphics and present queues is always required.
        // I'll support headless compute-only devices in the future, but for now this is a graphics app.
        // requiredQueues: in addition to the graphics and present, but prefers dedicated families.
        // preferredDevice: devices of this type are chosen first; otherwise discrete > integrated > virtual > CPU.
        Foundry(
            vk::raii::Instance const &instance,
            vk::raii::SurfaceKHR windowSurface,
            vk::QueueFlags requiredQueues = vk::QueueFlagBits::eGraphics,
            vk::PhysicalDeviceType preferredDevice = vk::PhysicalDeviceType::eDiscreteGpu,
            std::vector<char const*> const &extensions = {},
            void const *features = nullptr);

        ~Foundry();

        vk::raii::PhysicalDevice const &physicalDevice() const { return m_physicalDevice; }
        vk::raii::SurfaceKHR const &surface() const { return m_surface; }
        vk::raii::Device const &device() const { return m_device; }

        // For the moment there is always a graphics and present queue, because they are needed for the Presenter.
        vk::raii::CommandPool const &commandPool() const { return m_commandPool; }
        vk::raii::Queue const &graphicsQueue() const { return m_graphicsQueue; }
        vk::raii::Queue const &presentQueue() const { return m_presentQueue; }

        // Optional queues: null unless the requiredQueues has specified them.
        vk::raii::CommandPool const &computeCommandPool() const { return m_computeCommandPool; }
        vk::raii::CommandPool const &transferCommandPool() const { return m_transferCommandPool; }
        vk::raii::Queue const &computeQueue() const { return m_computeQueue; }
        vk::raii::Queue const &transferQueue() const { return m_transferQueue; }
        vk::raii::Queue const &videoDecodeQueue() const { return m_videoDecodeQueue; }

        QueueFamilies const &queueFamilies() const { return m_queueFamilies; }

        // The device extensions the device was created with, including the ones the Foundry always adds.
        std::vector<std::string> const &extensions() const { return m_extensions; }

        // Create a swapchain for the surface.
        // desiredImageCount: clamped to what the surface supports for the Swapchain imageCount.
        // oldSwapchain: the current swapchain so the driver can reuse its resources.  The caller must still destroy it.
        SwapchainInfo createSwapchain(
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t desiredImageCount,
            vk::SwapchainKHR oldSwapchain = {}) const;

        void waitIdle() const;

        // For destructors, which must not throw: if the device has been lost there's nothing left to wait for.
        void waitIdleNoThrow() const noexcept;

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
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::SurfaceKHR m_surface{nullptr};
        vk::raii::Device m_device{nullptr};

        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::CommandPool m_computeCommandPool{nullptr};
        vk::raii::CommandPool m_transferCommandPool{nullptr};

        vk::raii::Queue m_graphicsQueue{nullptr};
        vk::raii::Queue m_presentQueue{nullptr};
        vk::raii::Queue m_computeQueue{nullptr};
        vk::raii::Queue m_transferQueue{nullptr};
        vk::raii::Queue m_videoDecodeQueue{nullptr};

        QueueFamilies m_queueFamilies;
        std::vector<std::string> m_extensions;
    };
}

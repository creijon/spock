// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "presenter.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

namespace spock
{
    // Responsible for managing the lifetime of the devices and command pool, and keeping track of the pool families.
    // Helps to simplify the calling convention for the wrappers, presenter, render pass etc.  But doesn't own them.
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

        uint32_t graphicsFamily() const { return m_graphicsFamily; }
        uint32_t presentFamily() const { return m_presentFamily; }
        uint32_t computeFamily() const { return m_computeFamily; }
        uint32_t transferFamily() const { return m_transferFamily; }

        std::vector<vk::SurfaceFormatKHR> getSurfaceFormatsKHR() const;
        vk::SurfaceCapabilitiesKHR getSurfaceCapabilitiesKHR() const;
        std::vector<vk::PresentModeKHR> getSurfacePresentModesKHR() const;

        void waitIdle() const;

    private:
        std::vector<vk::DeviceQueueCreateInfo> uniqueCreateInfos() const;

        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::SurfaceKHR m_surface{nullptr};
        vk::raii::Device m_device{nullptr};
        vk::raii::CommandPool m_commandPool{nullptr};

        uint32_t m_graphicsFamily{0};
        uint32_t m_presentFamily{0};
        uint32_t m_computeFamily{0};
        uint32_t m_transferFamily{0};
    };
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "foundry.hpp"

#include "creators.hpp"

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>
#endif

namespace
{
    std::vector<std::string> deviceExtensions(std::vector<std::string> const& extensions)
    {
        std::vector<std::string> defaultExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        };

        defaultExtensions.insert(defaultExtensions.end(), extensions.begin(), extensions.end());

        return defaultExtensions;
    }
}

namespace spock
{
    Foundry::Foundry(
        vk::raii::Instance const &instance,
        vk::raii::SurfaceKHR windowSurface,
        std::vector<std::string> const &extensions,
        void const *features)
        : m_physicalDevice(vk::raii::PhysicalDevices(instance).front())
        , m_windowSurface(std::move(windowSurface))
    {
        m_queues = Queues(m_physicalDevice, m_windowSurface);
        m_device = createDevice(
            m_physicalDevice, 
            m_queues,
            deviceExtensions(extensions),
            nullptr,
            features);

        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_queues.graphicsFamily()};
        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);
    }

    Foundry::~Foundry()
    {
        m_device.waitIdle();
    }

    void Foundry::waitIdle() const
    {
        m_device.waitIdle();
    }
}

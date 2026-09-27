// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "foundry.hpp"

#include "creators.hpp"

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>
#endif

#include <optional>
#include <set>

namespace spock
{
    // Find the first queue family whose flags include every flag in `include` and exclude every
    // flag in `exclude`.
    // If none are found return an empty optional.
    std::optional<uint32_t> findQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude)
    {
        auto queueFamilyProperty =
            std::find_if(
                queueFamilyProperties.begin(),
                queueFamilyProperties.end(),
                [=](vk::QueueFamilyProperties const &qfp)
                { return (qfp.queueFlags & include) == include && !(qfp.queueFlags & exclude); });

        if (queueFamilyProperty == queueFamilyProperties.end())
        {
            return std::nullopt;
        }
        
        return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), queueFamilyProperty));
    }

    // Find the first queue family whose flags include every flag in `include` and exclude every
    // flag in `exclude`.
    // If this can't be found, then return the first regardless of the `exclude` flags.
    // If none are found return an empty optional.
    std::optional<uint32_t> findQueueFamilyIndexFallback(
        std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude)
    {
        auto queueFamily = findQueueFamilyIndex(
            queueFamilyProperties,
            include, exclude);
        if (!queueFamily.has_value())
        {
            queueFamily = findQueueFamilyIndex(
                queueFamilyProperties,
                include, {});
        }

        return queueFamily;
    }

    // Find queue family indices for graphics and presentation. If a single queue family supports
    // both, the same index is returned for both.
    std::pair<uint32_t, uint32_t> findGraphicsAndPresentQueueFamily(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::SurfaceKHR const& surface,
        std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties)
    {
        auto graphicsQueueFamilyIndex = findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics, {});

        if (!graphicsQueueFamilyIndex.has_value())
        {
            throw std::runtime_error("Could not find a queue family that supports graphics, terminating.");
        }

        const uint32_t graphicsFamily = graphicsQueueFamilyIndex.value();

        if (physicalDevice.getSurfaceSupportKHR(graphicsFamily, surface))
        {
            // The graphics family also supports present.
            return { graphicsFamily, graphicsFamily };
        }

        // The graphics family doesn't support present, so look for another family index that
        // supports both graphics and present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return { graphicsFamily, i };
            }
        }

        // There's no single family that supports both graphics and present, so look for another
        // family index that supports present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return { graphicsFamily, i };
            }
        }

        throw std::runtime_error("Could not find a queue family that supports present, terminating.");
        return { };
    }

    Foundry::Foundry(
        vk::raii::Instance const &instance,
        vk::raii::SurfaceKHR windowSurface,
        std::vector<char const*> const &extensions,
        void const *features)
        : m_physicalDevice(vk::raii::PhysicalDevices(instance).front())
        , m_surface(std::move(windowSurface))
    {
        // Find the queue families.
        auto queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        std::tie(m_graphicsFamily, m_presentFamily) = findGraphicsAndPresentQueueFamily(
            m_physicalDevice,
            m_surface,
            queueFamilyProperties);

        auto computeFamilyIndex = findQueueFamilyIndexFallback(
            queueFamilyProperties,
            vk::QueueFlagBits::eCompute,
            vk::QueueFlagBits::eGraphics);
        m_computeFamily = (computeFamilyIndex.has_value()) ? computeFamilyIndex.value() : m_graphicsFamily;

        auto transferFamilyIndex = findQueueFamilyIndexFallback(
            queueFamilyProperties,
            vk::QueueFlagBits::eTransfer,
            vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
        m_transferFamily = (transferFamilyIndex.has_value()) ? transferFamilyIndex.value() : m_graphicsFamily;

        // Create the device.
        std::vector<char const*> deviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        };
        deviceExtensions.insert(deviceExtensions.end(), extensions.begin(), extensions.end());

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{ uniqueCreateInfos() };

        vk::DeviceCreateInfo deviceCreateInfo(
            vk::DeviceCreateFlags(),
            queueCreateInfos,
            {},
            deviceExtensions,
            nullptr,
            features);

        m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
        m_graphicsQueue = vk::raii::Queue(m_device, m_graphicsFamily, 0);
        m_presentQueue = vk::raii::Queue(m_device, m_presentFamily, 0);

        // Create the command pool.
        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_graphicsFamily};
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

    std::vector<vk::SurfaceFormatKHR> Foundry::getSurfaceFormatsKHR() const
    {
        return m_physicalDevice.getSurfaceFormatsKHR(m_surface);
    }

    vk::SurfaceCapabilitiesKHR Foundry::getSurfaceCapabilitiesKHR() const
    {
        return m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
    }

    std::vector<vk::PresentModeKHR> Foundry::getSurfacePresentModesKHR() const
    {
        return m_physicalDevice.getSurfacePresentModesKHR(m_surface);
    }

    std::vector<vk::DeviceQueueCreateInfo> Foundry::uniqueCreateInfos() const
    {
        float queuePriority = 0.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            m_graphicsFamily,
            m_presentFamily,
            m_computeFamily,
            m_transferFamily
        };

        for (uint32_t familyIndex : uniqueQueueFamilies) {
            queueCreateInfos.emplace_back(
                vk::DeviceQueueCreateInfo{}
                .setQueueFamilyIndex(familyIndex)
                .setQueueCount(1)
                .setPQueuePriorities(&queuePriority)
            );
        }

        return queueCreateInfos;
    }
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/foundry.hpp"

#include "spock/device_selector.hpp"

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>
#endif

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace spock
{

    std::vector<vk::DeviceQueueCreateInfo> uniqueQueueCreateInfos(QueueFamilies const &queueFamilies)
    {
        // The returned create infos point at this value, so it must outlive the function.
        // 1.0 is the conventional priority for a single queue per family.
        static constexpr float queuePriority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            queueFamilies.graphics,
            queueFamilies.present,
            queueFamilies.compute,
            queueFamilies.transfer
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

    Foundry::Foundry(
        vk::raii::Instance const &instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::QueueFlags requiredQueues,
        vk::PhysicalDeviceType preferredDevice,
        std::vector<char const*> const &extensions,
        void const *features)
        : m_surface(std::move(windowSurface))
    {
        std::vector<char const*> deviceExtensions{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        };
        deviceExtensions.insert(deviceExtensions.end(), extensions.begin(), extensions.end());

        std::tie(m_physicalDevice, m_queueFamilies) = selectDevice(
            instance,
            *m_surface,
            requiredQueues,
            preferredDevice,
            deviceExtensions);

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{ uniqueQueueCreateInfos(m_queueFamilies) };

        vk::DeviceCreateInfo deviceCreateInfo(
            vk::DeviceCreateFlags(),
            queueCreateInfos,
            {},
            deviceExtensions,
            nullptr,
            features);

        m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
        m_graphicsQueue = vk::raii::Queue(m_device, m_queueFamilies.graphics, 0);
        m_presentQueue = vk::raii::Queue(m_device, m_queueFamilies.present, 0);
        m_computeQueue = vk::raii::Queue(m_device, m_queueFamilies.compute, 0);
        m_transferQueue = vk::raii::Queue(m_device, m_queueFamilies.transfer, 0);

        // Create the command pools.
        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_queueFamilies.graphics};
        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        poolInfo.queueFamilyIndex = m_queueFamilies.compute;
        m_computeCommandPool = vk::raii::CommandPool(m_device, poolInfo);

        poolInfo.queueFamilyIndex = m_queueFamilies.transfer;
        m_transferCommandPool = vk::raii::CommandPool(m_device, poolInfo);
    }

    Foundry::~Foundry()
    {
        m_device.waitIdle();
    }

    void Foundry::waitIdle() const
    {
        m_device.waitIdle();
    }

    SwapchainInfo Foundry::createSwapchain(
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t desiredImageCount,
        vk::SwapchainKHR oldSwapchain) const
    {
        vk::SurfaceFormatKHR surfaceFormat = pickSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(m_surface));

        vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
        vk::Extent2D swapchainExtent;
        if (surfaceCapabilities.currentExtent.width == (std::numeric_limits<uint32_t>::max)())
        {
            // If the surface size is undefined, the size is set to the size of the images requested.
            swapchainExtent.width = std::clamp(
                extent.width,
                surfaceCapabilities.minImageExtent.width,
                surfaceCapabilities.maxImageExtent.width);
            swapchainExtent.height = std::clamp(
                extent.height,
                surfaceCapabilities.minImageExtent.height,
                surfaceCapabilities.maxImageExtent.height);
        }
        else
        {
            // If the surface size is defined, the swap chain size must match
            swapchainExtent = surfaceCapabilities.currentExtent;
        }

        auto preTransform =
            (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) ?
            vk::SurfaceTransformFlagBitsKHR::eIdentity :
            surfaceCapabilities.currentTransform;

        using Alpha = vk::CompositeAlphaFlagBitsKHR;
        auto compositeAlpha =
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::eOpaque)         ? Alpha::eOpaque :
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::ePreMultiplied)  ? Alpha::ePreMultiplied :
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::ePostMultiplied) ? Alpha::ePostMultiplied :
            Alpha::eInherit;

        vk::PresentModeKHR presentMode = pickPresentMode(m_physicalDevice.getSurfacePresentModesKHR(m_surface));
        uint32_t imageCount = clampSurfaceImageCount(desiredImageCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
        vk::SwapchainCreateInfoKHR swapChainCreateInfo(
            {},
            m_surface,
            imageCount,
            surfaceFormat.format,
            surfaceFormat.colorSpace,
            swapchainExtent,
            1,
            usage,
            vk::SharingMode::eExclusive,
            {},
            preTransform,
            compositeAlpha,
            presentMode,
            true,
            oldSwapchain);

        uint32_t const queueFamilyIndices[]{m_queueFamilies.graphics, m_queueFamilies.present};
        if (m_queueFamilies.graphics != m_queueFamilies.present)
        {
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        return {
            vk::raii::SwapchainKHR(m_device, swapChainCreateInfo),
            surfaceFormat.format,
            swapchainExtent};
    }
}

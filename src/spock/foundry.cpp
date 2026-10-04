// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "foundry.hpp"

#include "creators.hpp"
#include "utils.hpp"

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>
#endif

#include <algorithm>
#include <cassert>
#include <limits>
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
                return { i, i };
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
    }

    bool checkDeviceExtensionSupport(
        vk::raii::PhysicalDevice const& physicalDevice,
        std::vector<char const*> const& requiredExtensions)
    {
        std::set<std::string> requiredExtensionsSet(requiredExtensions.begin(), requiredExtensions.end());
        auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        for (const auto& extension : availableExtensions)
        {
            requiredExtensionsSet.erase(extension.extensionName);
        }
        return requiredExtensionsSet.empty();
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
        // Create the device.
        std::vector<char const*> deviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        };
        deviceExtensions.insert(deviceExtensions.end(), extensions.begin(), extensions.end());

        selectPhysicalDeviceAndQueueFamilies(instance, requiredQueues, preferredDevice, deviceExtensions);

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
        m_computeQueue = vk::raii::Queue(m_device, m_computeFamily, 0);
        m_transferQueue = vk::raii::Queue(m_device, m_transferFamily, 0);

        // Create the command pools.
        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_graphicsFamily};
        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        poolInfo.queueFamilyIndex = m_computeFamily;
        m_computeCommandPool = vk::raii::CommandPool(m_device, poolInfo);

        poolInfo.queueFamilyIndex = m_transferFamily;
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

    std::vector<vk::DeviceQueueCreateInfo> Foundry::uniqueCreateInfos() const
    {
        // The returned create infos point at this value, so it must outlive the function.
        // 1.0 is the conventional priority for a single queue per family.
        static constexpr float queuePriority = 1.0f;
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

        uint32_t const queueFamilyIndices[]{m_graphicsFamily, m_presentFamily};
        if (m_graphicsFamily != m_presentFamily)
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

    struct PhysicalDeviceScore
    {
        vk::raii::PhysicalDevice device;
        uint32_t graphicsFamily{ 0 };
        uint32_t presentFamily{ 0 };
        uint32_t computeFamily{ 0 };
        uint32_t transferFamily{ 0 };
        uint32_t rank{ 0 };
    };

    void Foundry::selectPhysicalDeviceAndQueueFamilies(
        vk::raii::Instance const& instance,
        vk::QueueFlags requiredQueues,
        vk::PhysicalDeviceType preferredDevice,
        std::vector<char const*> const& requiredExtensions)
    {
        auto physicalDevices = vk::raii::PhysicalDevices(instance);
        std::vector<PhysicalDeviceScore> deviceScores;

        for (const auto& device : physicalDevices)
        {
            if (!checkDeviceExtensionSupport(device, requiredExtensions))
            {
                // Only consider devices that support the required extensions.
                continue;
            }

            PhysicalDeviceScore score{ device };
            auto queueFamilyProperties = device.getQueueFamilyProperties();
            assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

            // Graphics and present are always required, because the Foundry always creates a swapchain.
            try
            {
                std::tie(score.graphicsFamily, score.presentFamily) = findGraphicsAndPresentQueueFamily(
                    device,
                    m_surface,
                    queueFamilyProperties);
            }
            catch (const std::runtime_error&)
            {
                // If we can't find graphics and present queue families, skip this device.
                continue;
            }

            // A requested compute queue must exist on the device, and a dedicated family scores higher.
            score.computeFamily = score.graphicsFamily;
            if (requiredQueues & vk::QueueFlagBits::eCompute)
            {
                auto dedicatedCompute = findQueueFamilyIndex(
                    queueFamilyProperties,
                    vk::QueueFlagBits::eCompute,
                    vk::QueueFlagBits::eGraphics);
                auto anyCompute = findQueueFamilyIndex(
                    queueFamilyProperties,
                    vk::QueueFlagBits::eCompute,
                    {});

                if (dedicatedCompute.has_value())
                {
                    score.computeFamily = dedicatedCompute.value();
                    score.rank += 1;
                }
                else if (queueFamilyProperties[score.graphicsFamily].queueFlags & vk::QueueFlagBits::eCompute)
                {
                    score.computeFamily = score.graphicsFamily;
                }
                else if (anyCompute.has_value())
                {
                    score.computeFamily = anyCompute.value();
                }
                else
                {
                    continue;
                }
            }

            // Graphics families always support transfer, so we only need to look for a dedicated family.
            score.transferFamily = score.graphicsFamily;
            if (requiredQueues & vk::QueueFlagBits::eTransfer)
            {
                auto dedicatedTransfer = findQueueFamilyIndex(
                    queueFamilyProperties,
                    vk::QueueFlagBits::eTransfer,
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
                if (dedicatedTransfer.has_value())
                {
                    score.transferFamily = dedicatedTransfer.value();
                    score.rank += 1;
                }
            }

            vk::PhysicalDeviceType deviceType = device.getProperties().deviceType;
            if (deviceType == preferredDevice)
            {
                // The preferred device type always wins.
                score.rank += 100;
            }
            else
            {
                // Otherwise fall back in order of expected performance.
                score.rank += (deviceType == vk::PhysicalDeviceType::eDiscreteGpu) ? 40 :
                              (deviceType == vk::PhysicalDeviceType::eIntegratedGpu) ? 30 :
                              (deviceType == vk::PhysicalDeviceType::eVirtualGpu) ? 20 :
                              (deviceType == vk::PhysicalDeviceType::eCpu) ? 10 : 0;
            }

            deviceScores.push_back(score);
        }

        if (deviceScores.empty())
        {
            throw std::runtime_error("No suitable physical device found.");
        }

        // Select the device with the highest score.
        auto bestDevice = std::max_element(deviceScores.begin(), deviceScores.end(),
            [](const PhysicalDeviceScore& a, const PhysicalDeviceScore& b) {
                return a.rank < b.rank;
            });

        m_physicalDevice = bestDevice->device;
        m_graphicsFamily = bestDevice->graphicsFamily;
        m_presentFamily = bestDevice->presentFamily;
        m_computeFamily = bestDevice->computeFamily;
        m_transferFamily = bestDevice->transferFamily;

        // Log the selected device.
        writeLog("[Spock Vulkan] selected physical device: " + std::string(m_physicalDevice.getProperties().deviceName.data()) + "\n");
    }
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/device_selector.hpp"

#include "spock/utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

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
        vk::SurfaceKHR const& surface,
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

    std::pair<vk::raii::PhysicalDevice, QueueFamilies> selectDevice(
        vk::raii::Instance const &instance,
        vk::SurfaceKHR const& surface,
        vk::QueueFlags requiredQueues,
        vk::PhysicalDeviceType preferredDevice,
        std::vector<char const*> const &requiredExtensions)
    {
        auto physicalDevices = vk::raii::PhysicalDevices(instance);
        std::vector<std::tuple<uint32_t, vk::raii::PhysicalDevice, QueueFamilies>> deviceSelections;

        for (const auto& physicalDevice : physicalDevices)
        {
            if (!checkDeviceExtensionSupport(physicalDevice, requiredExtensions))
            {
                // Only consider devices that support the required extensions.
                continue;
            }

            QueueFamilies queueFamilies{};
            uint32_t rank = 0;
            auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
            assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

            // Graphics and present are always required, because the Foundry always creates a swapchain.
            try
            {
                std::tie(queueFamilies.graphics, queueFamilies.present) = findGraphicsAndPresentQueueFamily(
                    physicalDevice,
                    surface,
                    queueFamilyProperties);
            }
            catch (const std::runtime_error&)
            {
                // If we can't find graphics and present queue families, skip this device.
                continue;
            }

            // A requested compute queue must exist on the device, and a dedicated family scores higher.
            queueFamilies.compute = queueFamilies.graphics;
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
                    queueFamilies.compute = dedicatedCompute.value();
                    rank += 1;
                }
                else if (queueFamilyProperties[queueFamilies.graphics].queueFlags & vk::QueueFlagBits::eCompute)
                {
                    queueFamilies.compute = queueFamilies.graphics;
                }
                else if (anyCompute.has_value())
                {
                    queueFamilies.compute = anyCompute.value();
                }
                else
                {
                    continue;
                }
            }

            // Graphics families always support transfer, so we only need to look for a dedicated family.
            queueFamilies.transfer = queueFamilies.graphics;
            if (requiredQueues & vk::QueueFlagBits::eTransfer)
            {
                auto dedicatedTransfer = findQueueFamilyIndex(
                    queueFamilyProperties,
                    vk::QueueFlagBits::eTransfer,
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
                if (dedicatedTransfer.has_value())
                {
                    queueFamilies.transfer = dedicatedTransfer.value();
                    rank += 1;
                }
            }

            vk::PhysicalDeviceType deviceType = physicalDevice.getProperties().deviceType;
            if (deviceType == preferredDevice)
            {
                // The preferred device type always wins.
                rank += 100;
            }
            else
            {
                // Otherwise fall back in order of expected performance.
                rank += (deviceType == vk::PhysicalDeviceType::eDiscreteGpu) ? 40 :
                        (deviceType == vk::PhysicalDeviceType::eIntegratedGpu) ? 30 :
                        (deviceType == vk::PhysicalDeviceType::eVirtualGpu) ? 20 :
                        (deviceType == vk::PhysicalDeviceType::eCpu) ? 10 : 0;
            }

            deviceSelections.push_back({ rank, physicalDevice,queueFamilies });
        }

        if (deviceSelections.empty())
        {
            throw std::runtime_error("No suitable physical device found.");
        }

        // Select the device with the highest score.
        auto bestSelection = std::max_element(deviceSelections.begin(), deviceSelections.end(),
            [](const auto& a, const auto& b) {
                return std::get<uint32_t>(a) < std::get<uint32_t>(b);
            });

        // Log the selected device.
        auto bestDevice = std::get<vk::raii::PhysicalDevice>(*bestSelection);
        std::string deviceName{bestDevice.getProperties().deviceName.data()};
        writeLog("[Spock Vulkan] selected physical device: " + deviceName + "\n");

        return { bestDevice, std::get<QueueFamilies>(*bestSelection) };
    }
}

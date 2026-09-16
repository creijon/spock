// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "queues.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
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

    Queues::Queues(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface)
    {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        std::tie(m_graphicsFamily, m_presentFamily) = findGraphicsAndPresentQueueFamily(
            physicalDevice,
            surface,
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
    }

    std::vector<vk::DeviceQueueCreateInfo> Queues::uniqueCreateInfos() const
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
} // namespace spock

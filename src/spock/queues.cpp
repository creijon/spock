#include "queues.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace spock
{
    // Find the first queue family whose flags include every flag in `include`
    // and exclude every flag in `exclude`. Returns queueFamilyProperties.size()
    // if no family matches.
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


    // Find queue family indices for graphics and presentation. If a single
    // queue family supports both, the same index is returned for both.
    void Queues::findGraphicsAndPresentQueueFamily(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::SurfaceKHR const& surface,
        std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties)
    {
        auto graphicsQueueFamilyIndex = findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics, {});

        if (!graphicsQueueFamilyIndex.has_value())
        {
            throw std::runtime_error("Could not find a queue family that supports graphics -> terminating");
        }

        m_graphicsFamily = graphicsQueueFamilyIndex.value();

        if (physicalDevice.getSurfaceSupportKHR(m_graphicsFamily, surface))
        {
            // The graphics family also supports present.
            m_presentFamily = m_graphicsFamily;
            return;
        }

        // The graphics family doesn't support present, so look for another
        // family index that supports both graphics and present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                m_presentFamily = i;
                return;
            }
        }

        // There's no single family that supports both graphics and present,
        // so look for another family index that supports present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                m_presentFamily = i;
                return;
            }
        }

        throw std::runtime_error("Could not find queues for both graphics or present -> terminating");
    }

    Queues::Queues(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface)
    {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        findGraphicsAndPresentQueueFamily(physicalDevice, surface, queueFamilyProperties);

        auto compute = findQueueFamilyIndex(
            queueFamilyProperties,
            vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
        if (!compute.has_value())
        {
            compute = findQueueFamilyIndex(
                queueFamilyProperties,
                vk::QueueFlagBits::eCompute, {});
        }

        m_computeFamily = (compute.has_value()) ? compute.value() : m_graphicsFamily;

        auto transfer = findQueueFamilyIndex(
            queueFamilyProperties,
            vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
        if (!transfer.has_value())
        {
            transfer = findQueueFamilyIndex(
                queueFamilyProperties,
                vk::QueueFlagBits::eTransfer, {});
        }

        m_transferFamily = (transfer.has_value()) ? transfer.value() : m_graphicsFamily;
    }
} // namespace spock

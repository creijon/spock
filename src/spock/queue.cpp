#include "queue.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace spock
{
    // The following free functions are implementation details of Queue's
    // family discovery. They're kept as ordinary (non-static) free functions
    // rather than being declared in queue.hpp, but since they operate purely
    // on plain data they're worth unit-testing directly via a matching
    // forward declaration (see queue_tests.cpp).

    // Find the first queue family index that supports graphics commands.
    uint32_t findGraphicsQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties)
    {
        auto graphicsQueueFamilyProperty =
            std::find_if(
                queueFamilyProperties.begin(),
                queueFamilyProperties.end(),
                [](vk::QueueFamilyProperties const &qfp)
                { return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });
        assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
        return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
    }

    // Find the first queue family whose flags include every flag in `include`
    // and exclude every flag in `exclude`. Returns queueFamilyProperties.size()
    // if no family matches.
    uint32_t findQueueFamilyIndex(
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
        return queueFamilyProperty == queueFamilyProperties.end()
            ? static_cast<uint32_t>(queueFamilyProperties.size())
            : static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), queueFamilyProperty));
    }

    // Find a compute queue family, preferring one dedicated to compute (no
    // graphics) to allow async compute, and falling back to the graphics
    // family if nothing else supports compute.
    uint32_t findComputeQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        uint32_t graphicsFamily)
    {
        uint32_t index = findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
        if (index == queueFamilyProperties.size())
        {
            index = findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute, {});
        }
        return index == queueFamilyProperties.size() ? graphicsFamily : index;
    }

    // Find a transfer queue family, preferring one dedicated to transfer (no
    // graphics or compute) to allow async transfer, and falling back to the
    // graphics family if nothing else advertises transfer -- a family that
    // supports graphics or compute implicitly supports transfer even when it
    // doesn't advertise the bit, so that fallback is always safe.
    uint32_t findTransferQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        uint32_t graphicsFamily)
    {
        uint32_t index = findQueueFamilyIndex(
            queueFamilyProperties,
            vk::QueueFlagBits::eTransfer,
            vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
        if (index == queueFamilyProperties.size())
        {
            index = findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eTransfer, {});
        }
        return index == queueFamilyProperties.size() ? graphicsFamily : index;
    }

    // Find queue family indices for graphics and presentation. If a single
    // queue family supports both, the same index is returned for both.
    std::pair<uint32_t, uint32_t> findGraphicsAndPresentQueueFamilyIndices(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface,
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties)
    {
        uint32_t graphicsQueueFamilyIndex = findGraphicsQueueFamilyIndex(queueFamilyProperties);
        if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex, surface))
        {
            // The graphics family also supports present.
            return {graphicsQueueFamilyIndex, graphicsQueueFamilyIndex};
        }

        // The graphics family doesn't support present, so look for another
        // family index that supports both graphics and present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return {i, i};
            }
        }

        // There's no single family that supports both graphics and present,
        // so look for another family index that supports present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return {graphicsQueueFamilyIndex, i};
            }
        }

        throw std::runtime_error("Could not find queues for both graphics or present -> terminating");
    }

    Queue::Queue(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface)
    {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        std::tie(m_graphicsFamily, m_presentFamily) =
            findGraphicsAndPresentQueueFamilyIndices(physicalDevice, surface, queueFamilyProperties);
        m_computeFamily = findComputeQueueFamilyIndex(queueFamilyProperties, m_graphicsFamily);
        m_transferFamily = findTransferQueueFamilyIndex(queueFamilyProperties, m_graphicsFamily);
    }
} // namespace spock

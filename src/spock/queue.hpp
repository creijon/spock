#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace spock
{
    // Discovers the queue family indices a renderer needs: graphics, present,
    // compute, and transfer. When a physical device has no queue family
    // dedicated to compute or transfer, falls back to a family that supports
    // it alongside graphics, since every Vulkan implementation guarantees at
    // least one family supports all three.
    class Queue
    {
    public:
        Queue(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::SurfaceKHR const &surface);
        Queue() = default;

        uint32_t graphicsFamily() const
        {
            return m_graphicsFamily;
        }

        uint32_t presentFamily() const
        {
            return m_presentFamily;
        }

        uint32_t computeFamily() const
        {
            return m_computeFamily;
        }

        uint32_t transferFamily() const
        {
            return m_transferFamily;
        }

    private:
        uint32_t m_graphicsFamily{0};
        uint32_t m_presentFamily{0};
        uint32_t m_computeFamily{0};
        uint32_t m_transferFamily{0};
    };
} // namespace spock

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <limits>
#include <string>
#include <vector>

namespace spock
{
    // A simple structure to hold the graphics and present queue family indices.
    struct QueueIndices
    {
        uint32_t graphics;
        uint32_t present;
    };

    // The timeout used for waiting on frame fences during rendering.
    const uint64_t FenceTimeout = 100000000ull;

    template <typename TargetType, typename SourceType>
    TargetType checked_cast(SourceType value)
    {
        static_assert(sizeof(TargetType) <= sizeof(SourceType), "No need to cast from smaller to larger type!");
        static_assert(std::numeric_limits<SourceType>::is_integer, "Only integer types supported!");
        static_assert(!std::numeric_limits<SourceType>::is_signed, "Only unsigned types supported!");
        static_assert(std::numeric_limits<TargetType>::is_integer, "Only integer types supported!");
        static_assert(!std::numeric_limits<TargetType>::is_signed, "Only unsigned types supported!");
        assert(value <= (std::numeric_limits<TargetType>::max)());
        return static_cast<TargetType>(value);
    }

    void writeLog(const std::string& message);

    // Clamp the requested swapchain image count between the supported min and max.
    uint32_t clampSurfaceImageCount(
        uint32_t desiredImageCount,
        uint32_t minImageCount,
        uint32_t maxImageCount);

    // Record an image layout transition barrier for a single image.
    void setImageLayout(
        vk::CommandBuffer const &commandBuffer,
        vk::Image image,
        vk::Format format,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout);

    vk::raii::DeviceMemory allocateDeviceMemory(
        vk::raii::Device const &device,
        vk::PhysicalDeviceMemoryProperties const &memoryProperties,
        vk::MemoryRequirements const &memoryRequirements,
        vk::MemoryPropertyFlags memoryPropertyFlags);

    // Allocate a temporary command buffer and submit a single one-time command.
    template <typename Func>
    void oneTimeSubmit(
        vk::raii::Device const &device,
        vk::CommandPool const &commandPool,
        vk::Queue const &queue,
        Func const &func)
    {
        vk::CommandBuffer commandBuffer =
            device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1)).front();
        commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        func(commandBuffer);
        commandBuffer.end();
        queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &commandBuffer), nullptr);
        queue.waitIdle();
    }

    // Find queue family indices for graphics and present queues.
    // The returned pair is {graphicsQueueFamilyIndex, presentQueueFamilyIndex}.
    QueueIndices findGraphicsAndPresentQueueFamilyIndex(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface);

    // Choose a surface format from the available list, preferring SRGB color space.
    vk::SurfaceFormatKHR pickSurfaceFormat(
        std::vector<vk::SurfaceFormatKHR> const &formats);

    vk::PresentModeKHR pickPresentMode(
        std::vector<vk::PresentModeKHR> const &presentModes);

    VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
        vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
        [[maybe_unused]] void *pUserData);

    vk::DebugUtilsMessengerCreateInfoEXT makeDebugUtilsMessengerCreateInfoEXT();

    template <typename T>
    void copyToDevice(
        vk::raii::DeviceMemory const &deviceMemory,
        T const *pData,
        size_t count,
        vk::DeviceSize stride = sizeof(T))
    {
        assert(sizeof(T) <= stride);
        uint8_t *deviceData = static_cast<uint8_t *>(deviceMemory.mapMemory(0, count * stride));
        if (stride == sizeof(T))
        {
            memcpy(deviceData, pData, count * sizeof(T));
        }
        else
        {
            for (size_t i = 0; i < count; i++)
            {
                memcpy(deviceData, &pData[i], sizeof(T));
                deviceData += stride;
            }
        }
        deviceMemory.unmapMemory();
    }

    // Copy a single value into device memory.
    template <typename T>
    void copyToDevice(
        vk::raii::DeviceMemory const &deviceMemory,
        T const &data)
    {
        copyToDevice<T>(deviceMemory, &data, 1);
    }
}

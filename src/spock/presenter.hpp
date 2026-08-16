
#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <vector>

namespace spock
{
    class Presenter
    {
    public:
        Presenter(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::raii::SurfaceKHR const &surface,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t graphicsQueueFamilyIndex,
            uint32_t presentQueueFamilyIndex,
            uint32_t framesInFlight);
        Presenter() = default;
        Presenter(const Presenter &) = delete;
        Presenter(Presenter && other) noexcept;
        Presenter const& operator=(Presenter && other);

        void initialise(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::raii::SurfaceKHR const &surface,
            vk::Extent2D const &extent,
            vk::ImageUsageFlags usage,
            uint32_t graphicsQueueFamilyIndex,
            uint32_t presentQueueFamilyIndex,
            uint32_t framesInFlight);

        std::vector<vk::raii::ImageView> const& imageViews() const
        {
            return m_imageViews;
        }

        uint32_t imageIndex() const
        {
            return m_imageIndex;
        }

        bool isValid() const
        {
            return m_valid;
        }

        void acquireFrame(vk::raii::Device const &device);
        void submitCommands(vk::raii::CommandBuffer const& commandBuffer);
        vk::Result presentFrame();

    private:
        vk::Format m_colorFormat;
    
        vk::raii::SwapchainKHR m_swapchain{nullptr};
        vk::raii::Queue m_graphicsQueue{nullptr};
        vk::raii::Queue m_presentQueue{nullptr};
    
        std::vector<vk::Image> m_images;
        std::vector<vk::raii::ImageView> m_imageViews;
        uint32_t m_imageIndex{0};

        std::vector<vk::raii::Semaphore> m_imageSemaphores;
        std::vector<vk::raii::Semaphore> m_renderSemaphores;
        std::vector<vk::raii::Fence> m_frameFences;
        uint32_t m_inFlightIndex{0};

        bool m_valid{false};
    };
} // namespace spock

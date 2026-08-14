#include "presenter.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <iostream>

using namespace vk;

namespace spock
{
    Presenter::Presenter(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::raii::SurfaceKHR const &surface,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t graphicsQueueFamilyIndex,
        uint32_t presentQueueFamilyIndex,
        uint32_t framesInFlight)
    {
        initialise(
            physicalDevice,
            device,
            surface,
            extent,
            usage,
            graphicsQueueFamilyIndex,
            presentQueueFamilyIndex,
            framesInFlight);
    }

    Presenter::Presenter(Presenter&&other) noexcept
        : m_colorFormat(other.m_colorFormat)
        , m_swapchain(std::move(other.m_swapchain))
        , m_queue(std::move(other.m_queue))
        , m_images(std::move(other.m_images))
        , m_imageViews(std::move(other.m_imageViews))
        , m_imageIndex(other.m_imageIndex)
        , m_framesInFlight(other.m_framesInFlight)
        , m_valid(other.m_valid)
    {
    }

    Presenter const& Presenter::operator=(Presenter&& other)
    {
        if (this != &other)
        {
            m_colorFormat = other.m_colorFormat;
            m_swapchain = std::move(other.m_swapchain);
            m_queue = std::move(other.m_queue);
            m_images = std::move(other.m_images);
            m_imageViews = std::move(other.m_imageViews);
            m_imageIndex = other.m_imageIndex;
            m_framesInFlight = other.m_framesInFlight;
            m_valid = other.m_valid;
        }

        return *this;
    }

    void Presenter::initialise(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::raii::SurfaceKHR const &surface,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t graphicsQueueFamilyIndex,
        uint32_t presentQueueFamilyIndex,
        uint32_t framesInFlight)
    {
        vk::SurfaceFormatKHR surfaceFormat = pickSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
        m_colorFormat = surfaceFormat.format;

        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        vk::Extent2D swapchainExtent;
        if (surfaceCapabilities.currentExtent.width == (std::numeric_limits<uint32_t>::max)())
        {
            // If the surface size is undefined, the size is set to the size of the images requested.
            swapchainExtent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            swapchainExtent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        }
        else
        {
            // If the surface size is defined, the swap chain size must match
            swapchainExtent = surfaceCapabilities.currentExtent;
        }
        vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
                                                           ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                           : surfaceCapabilities.currentTransform;
        vk::CompositeAlphaFlagBitsKHR compositeAlpha =
            (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)    ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                             : vk::CompositeAlphaFlagBitsKHR::eOpaque;
        vk::PresentModeKHR presentMode = pickPresentMode(physicalDevice.getSurfacePresentModesKHR(surface));
        vk::SwapchainKHR prevSwapchain = *m_swapchain;
        uint32_t imageCount = clampSurfaceImageCount(m_framesInFlight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
        vk::SwapchainCreateInfoKHR swapChainCreateInfo({},
                                                       surface,
                                                       imageCount,
                                                       m_colorFormat,
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
                                                       prevSwapchain);
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex)
        {
            uint32_t queueFamilyIndices[2]{graphicsQueueFamilyIndex, presentQueueFamilyIndex};
            // If the graphics and present queues are from different queue families, we either have to explicitly
            // transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
            // as vk::SharingMode::eConcurrent
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        m_swapchain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);

        m_images = m_swapchain.getImages();

        vk::ImageViewCreateInfo imageViewCreateInfo{
            {},
            {},
            vk::ImageViewType::e2D,
            m_colorFormat,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        m_imageViews.reserve(m_images.size());
        m_imageViews.clear();

        for (auto image : m_images)
        {
            imageViewCreateInfo.image = image;
            m_imageViews.emplace_back(device, imageViewCreateInfo);
        }

        m_queue = vk::raii::Queue(device, presentQueueFamilyIndex, 0);

        m_valid = true;
    }

    void Presenter::acquireFame(uint64_t fenceTimeout, vk::raii::Semaphore const& semaphore)
    {
        vk::Result result;

        std::tie(result, m_imageIndex) = m_swapchain.acquireNextImage(fenceTimeout, semaphore);

        if (result != vk::Result::eSuccess)
        {
            m_valid = false;
        }
        assert(m_imageIndex < m_images.size());
    }

    vk::Result Presenter::presentFrame(vk::raii::Semaphore const& semaphore)
    {
        // Present the rendered image to the swapchain.
        vk::PresentInfoKHR presentInfo;
        presentInfo.setWaitSemaphores(*semaphore);
        presentInfo.setSwapchains(*m_swapchain);
        presentInfo.setPImageIndices(&m_imageIndex);

        return m_queue.presentKHR(presentInfo);
    }


} // namespace spock
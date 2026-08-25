// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "presenter.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <iostream>
#include <limits>

using namespace vk;

namespace spock
{
    Presenter::Presenter(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::raii::SurfaceKHR const &surface,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        QueueIndices queueIndices,
        uint32_t framesInFlight)
    {
        initialise(
            physicalDevice,
            device,
            surface,
            extent,
            usage,
            queueIndices,
            framesInFlight);
    }

    Presenter::Presenter(Presenter&&other) noexcept
        : m_colorFormat(other.m_colorFormat)
        , m_swapchain(std::move(other.m_swapchain))
        , m_graphicsQueue(std::move(other.m_graphicsQueue))
        , m_presentQueue(std::move(other.m_presentQueue))
        , m_images(std::move(other.m_images))
        , m_imageViews(std::move(other.m_imageViews))
        , m_imageIndex(other.m_imageIndex)
        , m_imageSemaphores(std::move(other.m_imageSemaphores))
        , m_renderSemaphores(std::move(other.m_renderSemaphores))
        , m_frameFences(std::move(other.m_frameFences))
    {
    }

    Presenter const& Presenter::operator=(Presenter&& other)
    {
        if (this != &other)
        {
            m_colorFormat = other.m_colorFormat;
            m_swapchain = std::move(other.m_swapchain);
            m_graphicsQueue = std::move(other.m_graphicsQueue);
            m_presentQueue = std::move(other.m_presentQueue);
            m_images = std::move(other.m_images);
            m_imageViews = std::move(other.m_imageViews);
            m_imageIndex = other.m_imageIndex;
            m_imageSemaphores = std::move(other.m_imageSemaphores);
            m_renderSemaphores = std::move(other.m_renderSemaphores);
            m_frameFences = std::move(other.m_frameFences);
        }

        return *this;
    }

    void Presenter::initialise(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::raii::SurfaceKHR const &surface,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        QueueIndices queueIndices,
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

        auto preTransform = 
            (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) ?
            vk::SurfaceTransformFlagBitsKHR::eIdentity :
            surfaceCapabilities.currentTransform;

        using Alpha = vk::CompositeAlphaFlagBitsKHR;
        auto compositeAlpha =
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::ePreMultiplied)  ? Alpha::ePreMultiplied :
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::ePostMultiplied) ? Alpha::ePostMultiplied :
            (surfaceCapabilities.supportedCompositeAlpha & Alpha::eInherit)        ? Alpha::eInherit :
            Alpha::eOpaque;

        vk::PresentModeKHR presentMode = pickPresentMode(physicalDevice.getSurfacePresentModesKHR(surface));
        vk::SwapchainKHR prevSwapchain = *m_swapchain;
        uint32_t imageCount = clampSurfaceImageCount(framesInFlight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
        vk::SwapchainCreateInfoKHR swapChainCreateInfo(
            {},
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
        if (queueIndices.graphics != queueIndices.present)
        {
            // If the graphics and present queues are from different queue families, we either have to explicitly
            // transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
            // as vk::SharingMode::eConcurrent
            uint32_t queueFamilyIndices[]{queueIndices.graphics, queueIndices.present};
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

        m_graphicsQueue = vk::raii::Queue(device, queueIndices.graphics, 0);
        m_presentQueue = vk::raii::Queue(device, queueIndices.present, 0);

        // Synchronisation primitives.
        m_imageSemaphores.reserve(framesInFlight);
        m_renderSemaphores.reserve(framesInFlight);
        m_frameFences.reserve(framesInFlight);
        m_imageSemaphores.clear();
        m_renderSemaphores.clear();
        m_frameFences.clear();

        vk::FenceCreateInfo fenceInfo{vk::FenceCreateFlagBits::eSignaled};
        vk::SemaphoreCreateInfo semaphoreInfo{};

        for (size_t i = 0; i < framesInFlight; i++)
        {
            m_imageSemaphores.push_back(device.createSemaphore(semaphoreInfo));
            m_renderSemaphores.push_back(device.createSemaphore(semaphoreInfo));
            m_frameFences.push_back(device.createFence(fenceInfo));
        }
    }

    vk::Result Presenter::acquireFrame(vk::raii::Device const &device, uint32_t frameIndex)
    {
        vk::Result result = vk::Result::eSuccess;

        static const uint64_t fenceTimeout = 100000000ull;

        (void)device.waitForFences({ m_frameFences[frameIndex] }, VK_TRUE, fenceTimeout);

        try
        {    
            std::tie(result, m_imageIndex) = m_swapchain.acquireNextImage(fenceTimeout, m_imageSemaphores[frameIndex]);
        }
        catch (std::exception const& e)
        {
            // Most commonly vk::OutOfDateKHRError right after a resize.
            result = vk::Result::eErrorOutOfDateKHR;
        }

        // Always reset: submitCommands() below is called unconditionally by
        // the caller even on an invalid/stale frame, and vkQueueSubmit
        // requires the fence it's given to be unsignaled.
        device.resetFences({ m_frameFences[frameIndex] });

        return result;
    }

    vk::Result Presenter::submitCommands(vk::raii::CommandBuffer const& commandBuffer, uint32_t frameIndex)
    {
        vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submitInfo(
            *m_imageSemaphores[frameIndex],
            waitStages,
            *commandBuffer,
            *m_renderSemaphores[frameIndex]);

        m_graphicsQueue.submit(submitInfo, m_frameFences[frameIndex]);

        return vk::Result::eSuccess;
    }

    vk::Result Presenter::presentFrame(uint32_t frameIndex)
    {
        try
        {
            // Present the rendered image to the swapchain.
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphores(*m_renderSemaphores[frameIndex]);
            presentInfo.setSwapchains(*m_swapchain);
            presentInfo.setPImageIndices(&m_imageIndex);

            return m_presentQueue.presentKHR(presentInfo);
        }
        catch (std::exception const& e)
        {
            return vk::Result::eErrorOutOfDateKHR;
        }
    }
} // namespace spock
// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "presenter.hpp"

#include "foundry.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace spock
{
    Presenter::Presenter(
        std::shared_ptr<const Foundry> const &foundry,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t framesInFlight)
        : m_foundry(foundry)
    {
        vk::raii::Device const& device = foundry->device();

        vk::SurfaceFormatKHR surfaceFormat = pickSurfaceFormat(foundry->getSurfaceFormatsKHR());
        m_colorFormat = surfaceFormat.format;

        vk::SurfaceCapabilitiesKHR surfaceCapabilities = foundry->getSurfaceCapabilitiesKHR();
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

        vk::PresentModeKHR presentMode = pickPresentMode(foundry->getSurfacePresentModesKHR());
        uint32_t imageCount = clampSurfaceImageCount(framesInFlight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
        vk::SwapchainCreateInfoKHR swapChainCreateInfo(
            {},
            foundry->surface(),
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
            true);
        if (foundry->graphicsFamily() != foundry->presentFamily())
        {
            // If the graphics and present queues are from different queue families, we either have to explicitly
            // transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
            // as vk::SharingMode::eConcurrent
            uint32_t queueFamilyIndices[]{ foundry->graphicsFamily(), foundry->presentFamily()};
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

        for (const auto& image : m_images)
        {
            imageViewCreateInfo.image = image;
            m_imageViews.emplace_back(device, imageViewCreateInfo);
        }

        // Synchronisation primitives.
        // imageSemaphores and frameFences are indexed by frame index (caller's in-flight index).
        // renderSemaphores must be indexed by swapchain image index.
        m_imageSemaphores.reserve(framesInFlight);
        m_frameFences.reserve(framesInFlight);
        m_renderSemaphores.reserve(m_images.size());

        vk::FenceCreateInfo fenceInfo{vk::FenceCreateFlagBits::eSignaled};
        vk::SemaphoreCreateInfo semaphoreInfo{};

        for (size_t i = 0; i < framesInFlight; i++)
        {
            m_imageSemaphores.push_back(device.createSemaphore(semaphoreInfo));
            m_frameFences.push_back(device.createFence(fenceInfo));
        }

        // Create semaphores for each swapchain image
        for (size_t i = 0; i < m_images.size(); i++)
        {
            m_renderSemaphores.push_back(device.createSemaphore(semaphoreInfo));
        }
    }

    Presenter::Presenter(Presenter&&other) noexcept
        : m_foundry(std::move(other.m_foundry))
        , m_swapchain(std::move(other.m_swapchain))
        , m_colorFormat(other.m_colorFormat)
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
            m_foundry = std::move(other.m_foundry);
            m_swapchain = std::move(other.m_swapchain);
            m_colorFormat = other.m_colorFormat;
            m_images = std::move(other.m_images);
            m_imageViews = std::move(other.m_imageViews);
            m_imageIndex = other.m_imageIndex;
            m_imageSemaphores = std::move(other.m_imageSemaphores);
            m_renderSemaphores = std::move(other.m_renderSemaphores);
            m_frameFences = std::move(other.m_frameFences);
        }

        return *this;
    }

    vk::Result Presenter::acquireFrame(vk::raii::Device const &device, uint32_t frameIndex)
    {
        // Wait without a timeout. The command buffer for this frame index is about to be re-recorded,
        // so returning while the previous submission is still executing is never acceptable.
        vk::Result waitResult = device.waitForFences(
            { m_frameFences[frameIndex] },
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());
        if (waitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Presenter: waiting for the frame fence failed: " + vk::to_string(waitResult));
        }

        // The fence is deliberately not reset here. That happens in submitCommands(), immediately before
        // the submit that will signal it again. If the acquire below fails, the caller skips the submit,
        // and the fence stays signalled so the next wait on this frame index returns straight away.
        try
        {
            vk::Result result = vk::Result::eSuccess;
            std::tie(result, m_imageIndex) = m_swapchain.acquireNextImage(
                std::numeric_limits<uint64_t>::max(),
                m_imageSemaphores[frameIndex]);
            return result;
        }
        catch (std::exception const &)
        {
            // Most commonly vk::OutOfDateKHRError right after a resize.
            return vk::Result::eErrorOutOfDateKHR;
        }
    }

    vk::Result Presenter::submitCommands(vk::raii::CommandBuffer const& commandBuffer, uint32_t frameIndex)
    {
        vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submitInfo(
            *m_imageSemaphores[frameIndex],
            waitStages,
            *commandBuffer,
            *m_renderSemaphores[m_imageIndex]);

        m_foundry->device().resetFences({ m_frameFences[frameIndex] });
        m_foundry->graphicsQueue().submit(submitInfo, m_frameFences[frameIndex]);

        return vk::Result::eSuccess;
    }

    vk::Result Presenter::presentFrame(uint32_t frameIndex)
    {
        try
        {
            // Present the rendered image to the swapchain.
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphores(*m_renderSemaphores[m_imageIndex]);
            presentInfo.setSwapchains(*m_swapchain);
            presentInfo.setPImageIndices(&m_imageIndex);

            return m_foundry->presentQueue().presentKHR(presentInfo);
        }
        catch (std::exception const& e)
        {
            return vk::Result::eErrorOutOfDateKHR;
        }
    }
} // namespace spock
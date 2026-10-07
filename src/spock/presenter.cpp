// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "presenter.hpp"

#include "foundry.hpp"

#include <exception>
#include <limits>
#include <tuple>
#include <utility>

namespace spock
{
    Presenter::Presenter(
        std::shared_ptr<const Foundry> const &foundry,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t framesInFlight,
        Presenter const *oldPresenter)
        : m_foundry(foundry)
    {
        vk::SwapchainKHR oldSwapchain = oldPresenter ? *oldPresenter->m_swapchain : vk::SwapchainKHR{};
        SwapchainInfo swapchain = foundry->createSwapchain(extent, usage, framesInFlight, oldSwapchain);
        m_swapchain = std::move(swapchain.swapchain);
        m_colorFormat = swapchain.colorFormat;
        m_extent = swapchain.extent;
        m_images = m_swapchain.getImages();

        vk::ImageViewCreateInfo imageViewCreateInfo{
            {},
            {},
            vk::ImageViewType::e2D,
            m_colorFormat,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        m_imageViews.reserve(m_images.size());

        vk::raii::Device const& device = foundry->device();

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
        , m_extent(other.m_extent)
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
            m_extent = other.m_extent;
            m_images = std::move(other.m_images);
            m_imageViews = std::move(other.m_imageViews);
            m_imageIndex = other.m_imageIndex;
            m_imageSemaphores = std::move(other.m_imageSemaphores);
            m_renderSemaphores = std::move(other.m_renderSemaphores);
            m_frameFences = std::move(other.m_frameFences);
        }

        return *this;
    }

    vk::Result Presenter::acquireFrame(uint32_t frameIndex)
    {
        // Wait without a timeout. The command buffer for this frame index is about to be re-recorded,
        // so returning while the previous submission is still executing is never acceptable.
        vk::Result waitResult = m_foundry->device().waitForFences(
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
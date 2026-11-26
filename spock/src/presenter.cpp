// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/presenter.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/frame_state.hpp"

#include <chrono>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    constexpr std::chrono::nanoseconds ACQUIRE_TIMEOUT{std::chrono::seconds(1)};
}

namespace spock
{
    Presenter::Presenter(
        FoundryPtr const &foundry,
        vk::SurfaceKHR surface,
        vk::Extent2D const &extent,
        vk::raii::RenderPass const &renderPass,
        vk::Format depthFormat,
        vk::ImageUsageFlags usage,
        uint32_t framesInFlight,
        Presenter const* oldPresenter,
        SwapchainOptions const& options)
        : m_foundry(foundry)
    {
        vk::SwapchainKHR oldSwapchain = oldPresenter ? *oldPresenter->m_swapchain : vk::SwapchainKHR{};
        SwapchainInfo swapchain = foundry->createSwapchain(surface, extent, usage, framesInFlight, oldSwapchain, options);
        m_swapchain = std::move(swapchain.swapchain);
        m_colorFormat = swapchain.colorFormat;
        m_extent = swapchain.extent;
        m_preTransform = swapchain.preTransform;
        m_presentMode = swapchain.presentMode;
        m_compositeAlpha = swapchain.compositeAlpha;
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

        // render semaphores must be indexed by swapchain image index.
        m_renderSemaphores.reserve(m_images.size());

        for (size_t i = 0; i < m_images.size(); i++)
        {
            m_renderSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo{}));
        }

        bool const useDepthBuffer = depthFormat != vk::Format::eUndefined;
        if (useDepthBuffer)
        {
            m_depthBuffer = ImageWrapper(
                foundry,
                depthFormat,
                m_extent,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eDepth);
        }

        m_frameBuffers = createFramebuffers(
            device, renderPass, m_imageViews, useDepthBuffer ? &m_depthBuffer.imageView() : nullptr, m_extent);
    }

    vk::Result Presenter::acquireFrame(FrameState& frame)
    {
        try
        {
            vk::Result result = vk::Result::eSuccess;
            std::tie(result, frame.imageIndex) = m_swapchain.acquireNextImage(
                static_cast<uint64_t>(ACQUIRE_TIMEOUT.count()),
                *frame.semaphore);
            return result;
        }
        catch (vk::SystemError const& error)
        {
            // Most commonly vk::OutOfDateKHRError right after a resize, but surface and device loss end up here too.
            return static_cast<vk::Result>(error.code().value());
        }
    }

    FrameState::SemaphoreWait Presenter::acquireWait(FrameState const &frame) const
    {
        return {*frame.semaphore, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput};
    }

    FrameState::SemaphoreSignal Presenter::renderSignal(FrameState const &frame) const
    {
        // Indexed by image, because the presentation engine holds the semaphore until the image is acquired again.
        return {*m_renderSemaphores.at(frame.imageIndex), 0};
    }

    vk::Result Presenter::present(FrameState const &frame)
    {
        try
        {
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphores(*m_renderSemaphores.at(frame.imageIndex));
            presentInfo.setSwapchains(*m_swapchain);
            presentInfo.setPImageIndices(&frame.imageIndex);

            return m_foundry->presentQueue().presentKHR(presentInfo);
        }
        catch (vk::SystemError const& error)
        {
            // The frame was already submitted, so only presentation failed.
            return static_cast<vk::Result>(error.code().value());
        }
    }
} // namespace spock

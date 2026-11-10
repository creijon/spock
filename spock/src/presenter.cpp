// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/presenter.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/frame_state.hpp"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace spock
{
    Presenter::Presenter(
        FoundryPtr const &foundry,
        vk::Extent2D const &extent,
        vk::ImageUsageFlags usage,
        uint32_t framesInFlight,
        Presenter const* oldPresenter,
        SwapchainOptions const& options)
        : m_foundry(foundry)
    {
        vk::SwapchainKHR oldSwapchain = oldPresenter ? *oldPresenter->m_swapchain : vk::SwapchainKHR{};
        SwapchainInfo swapchain = foundry->createSwapchain(extent, usage, framesInFlight, oldSwapchain, options);
        m_swapchain = std::move(swapchain.swapchain);
        m_colorFormat = swapchain.colorFormat;
        m_extent = swapchain.extent;
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
    }

    Presenter::Presenter(Presenter&& other) noexcept
        : m_foundry(std::move(other.m_foundry))
        , m_renderSemaphores(std::move(other.m_renderSemaphores))
        , m_swapchain(std::move(other.m_swapchain))
        , m_colorFormat(other.m_colorFormat)
        , m_extent(other.m_extent)
        , m_presentMode(other.m_presentMode)
        , m_compositeAlpha(other.m_compositeAlpha)
        , m_images(std::move(other.m_images))
        , m_imageViews(std::move(other.m_imageViews))
        , m_frameBuffers(std::move(other.m_frameBuffers))
    {
    }

    Presenter const& Presenter::operator=(Presenter&& other)
    {
        if (this != &other)
        {
            // Retire dependent objects before dropping their views, swapchain, or device owner. The render
            // semaphores go after the swapchain, which is what completes the presentation waits on them.
            m_frameBuffers.clear();
            m_imageViews.clear();
            m_images.clear();
            m_swapchain = nullptr;
            m_renderSemaphores.clear();
            m_foundry = std::move(other.m_foundry);
            m_swapchain = std::move(other.m_swapchain);
            m_colorFormat = other.m_colorFormat;
            m_extent = other.m_extent;
            m_presentMode = other.m_presentMode;
            m_compositeAlpha = other.m_compositeAlpha;
            m_images = std::move(other.m_images);
            m_imageViews = std::move(other.m_imageViews);
            m_frameBuffers = std::move(other.m_frameBuffers);
            m_renderSemaphores = std::move(other.m_renderSemaphores);
        }

        return *this;
    }

    void Presenter::createFramebuffers(vk::raii::RenderPass const &renderPass, vk::raii::ImageView const *depthImageView)
    {
        m_frameBuffers = spock::createFramebuffers(m_foundry->device(), renderPass, m_imageViews, depthImageView, m_extent);
    }

    void Presenter::clearFramebuffers()
    {
        m_frameBuffers.clear();
    }

    vk::Result Presenter::acquireFrame(FrameState& frame, std::chrono::nanoseconds timeout)
    {
        try
        {
            vk::Result result = vk::Result::eSuccess;
            std::tie(result, frame.imageIndex) = m_swapchain.acquireNextImage(
                static_cast<uint64_t>((std::max)(timeout.count(), std::chrono::nanoseconds::rep(0))),
                *frame.semaphore);
            return result;
        }
        catch (vk::SystemError const& error)
        {
            // Most commonly vk::OutOfDateKHRError right after a resize, but surface and device loss end up here too.
            return static_cast<vk::Result>(error.code().value());
        }
    }

    vk::Result Presenter::presentFrame(FrameState &frame)
    {
        std::vector<vk::Semaphore> waitSemaphores{ *frame.semaphore };
        std::vector<vk::PipelineStageFlags> waitStages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        std::vector<uint64_t> waitValues{ 0 };
        for (FrameState::SemaphoreWait const &wait : frame.extraWaitSemaphores)
        {
            waitSemaphores.emplace_back(wait.semaphore);
            waitStages.emplace_back(wait.stageMask);
            waitValues.emplace_back(wait.value);
        }

        // The frame's timeline tells the FrameStatePool when the frame can be reused.
        std::vector<vk::Semaphore> signalSemaphores{ *m_renderSemaphores[frame.imageIndex], *frame.timeline };
        std::vector<uint64_t> signalValues{ 0, frame.timelineValue + 1 };
        for (FrameState::SemaphoreSignal const &signal : frame.extraSignalSemaphores)
        {
            signalSemaphores.emplace_back(signal.semaphore);
            signalValues.emplace_back(signal.value);
        }

        vk::SubmitInfo submitInfo(
            waitSemaphores,
            waitStages,
            *frame.commandBuffer,
            signalSemaphores);

        // The values are ignored for the binary swapchain semaphores.
        vk::TimelineSemaphoreSubmitInfo timelineSubmitInfo(waitValues, signalValues);
        submitInfo.pNext = &timelineSubmitInfo;

        frame.extraWaitSemaphores.clear();
        frame.extraSignalSemaphores.clear();

        m_foundry->graphicsQueue().submit(submitInfo);
        // Only advance once the submission has been made, so a failed submit leaves the pool nothing to wait for.
        ++frame.timelineValue;

        try
        {
            // Present the rendered image to the swapchain.
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphores(*m_renderSemaphores[frame.imageIndex]);
            presentInfo.setSwapchains(*m_swapchain);
            presentInfo.setPImageIndices(&frame.imageIndex);

            return m_foundry->presentQueue().presentKHR(presentInfo);
        }
        catch (vk::SystemError const& error)
        {
            // The submission went ahead, so the frame's timeline still advances; only presentation failed.
            return static_cast<vk::Result>(error.code().value());
        }
    }
} // namespace spock

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/presenter.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/frame_state.hpp"

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

        // render semaphores must be indexed by swapchain image index.
        m_renderSemaphores.reserve(m_images.size());

        for (size_t i = 0; i < m_images.size(); i++)
        {
            m_renderSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo{}));
        }
    }

    Presenter::Presenter(Presenter&&other) noexcept
        : m_foundry(std::move(other.m_foundry))
        , m_swapchain(std::move(other.m_swapchain))
        , m_colorFormat(other.m_colorFormat)
        , m_extent(other.m_extent)
        , m_images(std::move(other.m_images))
        , m_imageViews(std::move(other.m_imageViews))
        , m_frameBuffers(std::move(other.m_frameBuffers))
        , m_renderSemaphores(std::move(other.m_renderSemaphores))
    {
    }

    Presenter const& Presenter::operator=(Presenter&& other)
    {
        if (this != &other)
        {
            // Retire dependent objects before dropping their views, swapchain, or device owner.
            m_frameBuffers.clear();
            m_renderSemaphores.clear();
            m_imageViews.clear();
            m_images.clear();
            m_swapchain = nullptr;
            m_foundry = std::move(other.m_foundry);
            m_swapchain = std::move(other.m_swapchain);
            m_colorFormat = other.m_colorFormat;
            m_extent = other.m_extent;
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

    vk::Result Presenter::acquireFrame(FrameState &frame)
    {
        try
        {
            vk::Result result = vk::Result::eSuccess;
            std::tie(result, frame.imageIndex) = m_swapchain.acquireNextImage(
                std::numeric_limits<uint64_t>::max(),
                frame.semaphore);
            return result;
        }
        catch (std::exception const &)
        {
            // Most commonly vk::OutOfDateKHRError right after a resize.
            return vk::Result::eErrorOutOfDateKHR;
        }
    }

    vk::Result Presenter::submitCommands(FrameState &frame)
    {
        vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submitInfo(
            *frame.semaphore,
            waitStages,
            *frame.commandBuffer,
            *m_renderSemaphores[frame.imageIndex]);

        m_foundry->device().resetFences({ frame.fence });
        m_foundry->graphicsQueue().submit(submitInfo, frame.fence);

        return vk::Result::eSuccess;
    }

    vk::Result Presenter::presentFrame(FrameState &frame)
    {
        try
        {
            // Present the rendered image to the swapchain.
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphores(*m_renderSemaphores[frame.imageIndex]);
            presentInfo.setSwapchains(*m_swapchain);
            presentInfo.setPImageIndices(&frame.imageIndex);

            return m_foundry->presentQueue().presentKHR(presentInfo);
        }
        catch (std::exception const& e)
        {
            return vk::Result::eErrorOutOfDateKHR;
        }
    }
} // namespace spock

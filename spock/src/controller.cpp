// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/controller.hpp"

#include "spock/helpers.hpp"

#include <stdexcept>

namespace spock
{
    Controller::Controller(
        FoundryPtr const &foundry,
        vk::SurfaceKHR surface,
        vk::Extent2D const &extent,
        CreateRendererFunc const &createRenderer,
        uint32_t framesInFlight,
        SwapchainOptions const &swapchainOptions)
        : m_foundry(foundry)
        , m_framesInFlight(framesInFlight)
        , m_swapchainOptions(swapchainOptions)
        , m_surface(surface)
    {
        if (!m_surface)
        {
            throw std::invalid_argument("Controller: the surface is null");
        }

        vk::Format const colorFormat = pickSurfaceFormat(m_foundry->physicalDevice(), m_surface).format;
        m_renderer = createRenderer(colorFormat, m_framesInFlight);
        if (!m_renderer)
        {
            throw std::invalid_argument("Controller: the renderer factory returned nullptr");
        }

        createPresenter(extent);
    }

    Controller::~Controller()
    {
        m_foundry->waitIdleNoThrow();
    }

    void Controller::resizeWindow(vk::Extent2D const &extent)
    {
        if (!m_surface)
        {
            throw std::logic_error("Controller::resizeWindow: the surface has been released, call setSurface() instead");
        }
        // Frames in flight may still be drawing into the old Presenter's framebuffers.
        m_foundry->waitIdle();
        createPresenter(extent);
    }

    void Controller::releaseSurface()
    {
        // The swapchain must be destroyed before its surface, and the GPU may still be using it.
        m_foundry->waitIdle();
        m_presenter.reset();
        m_surface = nullptr;
    }

    void Controller::setSurface(vk::SurfaceKHR surface, vk::Extent2D const &extent)
    {
        if (!surface)
        {
            throw std::invalid_argument("Controller::setSurface: the surface is null");
        }
        if (pickSurfaceFormat(m_foundry->physicalDevice(), surface).format != m_renderer->colorFormat())
        {
            throw std::runtime_error("Controller::setSurface: the surface needs a different color format from the render pass");
        }
        // A swapchain can only be handed over to one for the same surface, so the old one is destroyed first.
        releaseSurface();
        m_surface = surface;
        createPresenter(extent);
    }

    void Controller::waitIdle() const
    {
        m_foundry->waitIdle();
    }

    void Controller::createPresenter(vk::Extent2D const &extent)
    {
        // The old presenter is kept alive until its replacement exists so its swapchain can be handed over.
        // Assigning the new one then destroys the old one, including its retired swapchain.
        m_presenter = std::make_unique<Presenter>(
            m_foundry,
            m_surface,
            extent,
            m_renderer->renderPass(),
            m_renderer->depthFormat(),
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            m_framesInFlight,
            m_presenter.get(),
            m_swapchainOptions);

        // The surface may constrain the swapchain extent, so the renderer uses the extent it was created with.
        m_renderer->setExtent(m_presenter->extent());
    }

    vk::Result Controller::renderFrame(std::chrono::microseconds frameTime)
    {
        if (!m_presenter)
        {
            // The surface has been released, so there is nothing to render to until setSurface().
            return vk::Result::eErrorSurfaceLostKHR;
        }

        if (!m_framePool)
        {
            m_framePool = std::make_unique<FrameStatePool>(m_foundry, m_framesInFlight, m_renderer->createFrameFunc());
        }

        FrameStateGuard frameGuard = m_framePool->acquireFrame();
        FrameState &frame = frameGuard.get();
        vk::Result acquireResult = m_presenter->acquireFrame(frame);

        // If image acquisition failed, skip rendering and presentation.
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            return acquireResult;
        }

        frame.commandBuffer.begin({});
        try
        {
            m_renderer->recordFrame(frame, m_presenter->framebuffer(frame.imageIndex), frameTime);
        }
        catch (...)
        {
            abandonFrame(frame);
            throw;
        }
        frame.commandBuffer.end();

        return m_presenter->presentFrame(frame);
    }

    void Controller::abandonFrame(FrameState &frame) noexcept
    {
        frame.extraWaitSemaphores.clear();
        frame.extraSignalSemaphores.clear();
        try
        {
            frame.commandBuffer.end();

            // An empty submission waits on the acquire semaphore and signals the timeline the pool waits on.
            vk::Semaphore const waitSemaphore = *frame.semaphore;
            vk::PipelineStageFlags const waitStage = vk::PipelineStageFlagBits::eAllCommands;
            vk::Semaphore const signalSemaphore = *frame.timeline;
            uint64_t const waitValue = 0;
            uint64_t const signalValue = frame.timelineValue + 1;
            vk::TimelineSemaphoreSubmitInfo timelineSubmitInfo(waitValue, signalValue);
            vk::SubmitInfo submitInfo(waitSemaphore, waitStage, {}, signalSemaphore, &timelineSubmitInfo);
            m_foundry->graphicsQueue().submit(submitInfo);
            ++frame.timelineValue;
        }
        catch (vk::SystemError const &)
        {
            // The original error is the one worth reporting.
        }
    }
} // namespace spock

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/renderer.hpp"

#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spock/presenter.hpp"

#include <stdexcept>

namespace spock
{
    Renderer::Renderer(
        FoundryPtr const &foundry,
        vk::SurfaceKHR surface,
        vk::Extent2D const &extents,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer,
        SwapchainOptions const& swapchainOptions)
        : m_foundry(foundry)
        , m_surface(surface)
        , m_depthFormat(useDepthBuffer ? vk::Format::eD16Unorm : vk::Format::eUndefined)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_swapchainOptions(swapchainOptions)
    {
        if (!m_surface)
        {
            throw std::invalid_argument("Renderer: the surface is null");
        }
        m_colorFormat = pickSurfaceFormat(m_foundry->physicalDevice(), m_surface).format;
        m_renderPass = createRenderPass(m_foundry->device(), m_colorFormat, m_depthFormat);
        buildSwapchain(extents);
    }

    Renderer::~Renderer()
    {
        m_foundry->waitIdleNoThrow();
    }

    void Renderer::resizeWindow(vk::Extent2D const &extents)
    {
        if (!m_surface)
        {
            throw std::logic_error("Renderer::resizeWindow: the surface has been released, call setSurface() instead");
        }
        // Destroying the frames waits for the GPU, so the old presenter can go once its replacement exists.
        m_framePool.reset();
        buildSwapchain(extents);
    }

    void Renderer::releaseSurface()
    {
        // The swapchain must be destroyed before its surface, and the GPU may still be using it.
        m_foundry->waitIdle();
        m_framePool.reset();
        m_presenter.reset();
        m_surface = nullptr;
    }

    void Renderer::setSurface(vk::SurfaceKHR surface, vk::Extent2D const &extents)
    {
        if (!surface)
        {
            throw std::invalid_argument("Renderer::setSurface: the surface is null");
        }
        if (pickSurfaceFormat(m_foundry->physicalDevice(), surface).format != m_colorFormat)
        {
            throw std::runtime_error("Renderer::setSurface: the surface needs a different color format from the render pass");
        }
        // A swapchain can only be handed over to one for the same surface, so the old one is destroyed first.
        releaseSurface();
        m_surface = surface;
        buildSwapchain(extents);
    }

    void Renderer::buildSwapchain(vk::Extent2D const &extents)
    {
        // The old presenter is kept alive until its replacement exists so its swapchain can be handed over.
        // Assigning the new one then destroys the old one, including its retired swapchain.
        m_presenter = std::make_unique<Presenter>(
            m_foundry,
            m_surface,
            extents,
            m_renderPass,
            m_depthFormat,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            m_framesInFlight,
            m_presenter.get(),
            m_swapchainOptions);

        // The surface may constrain the swapchain extent, so the viewport and scissor use the extent it was created with.
        // Frames are allocated by the next renderFrame, so a derived constructor can set m_createFrameFunc first.
        m_extents = m_presenter->extent();
    }

    void Renderer::waitIdle() const
    {
        m_foundry->waitIdle();
    }

    vk::Result Renderer::renderFrame(std::chrono::microseconds frameTime)
    {
        m_frameTime = frameTime;

        if (!m_presenter)
        {
            // The surface has been released, so there is nothing to render to until setSurface().
            return vk::Result::eErrorSurfaceLostKHR;
        }

        if (!m_framePool)
        {
            // If this is the first frame or the window has been resized, allocate the frames states.
            m_framePool = std::make_unique<FrameStatePool>(m_foundry, m_framesInFlight, m_createFrameFunc);
        }

        FrameStateGuard frameGuard = m_framePool->acquireFrame();
        FrameState &frameState = frameGuard.get();
        vk::Result acquireResult = m_presenter->acquireFrame(frameState, m_acquireTimeout);

        // If image acquisition failed, skip rendering and presentation.
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            return acquireResult;
        }

        frameState.commandBuffer.begin({});

        try
        {
            prepare(frameState);

			{
            	vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };
				vk::RenderPassBeginInfo renderPassBeginInfo(
					*m_renderPass,
					*m_presenter->framebuffer(frameState.imageIndex),
					vk::Rect2D(vk::Offset2D(0, 0), m_extents),
					clearValues);

				RenderPassWrapper renderPass(
                    frameState.commandBuffer,
                    renderPassBeginInfo);

                // Setup the viewport and scissor rectangle.
                frameState.commandBuffer.setViewport(
                    0, vk::Viewport(0.0f,
                        0.0f,
                        static_cast<float>(m_extents.width),
                        static_cast<float>(m_extents.height),
                        0.0f,
                        1.0f));
                frameState.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_extents));

                // The derived class renders using the current frame state.
                render(frameState);
            }
        }
        catch (...)
        {
            try
            {
                frameState.commandBuffer.end();
            }
            catch (vk::SystemError const &)
            {
            }
            throw;
        }

        frameState.commandBuffer.end();

        vk::Result result = m_presenter->presentFrame(frameState);

        m_frameCount++;

        return result;
    }
} // namespace spock

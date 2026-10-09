// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/renderer.hpp"

#include "spock/creators.hpp"
#include "spock/presenter.hpp"
#include "spock/helpers.hpp"

namespace spock
{
    Renderer::Renderer(
        FoundryPtr const &foundry,
        vk::Extent2D const &extents,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer)
        : m_foundry(foundry)
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
    {
        resizeWindow(extents);
    }

    Renderer::~Renderer()
    {
        waitIdle();
        // Renderer owns the render pass and depth attachment referenced by Presenter's framebuffers.
        if (m_presenter) m_presenter->clearFramebuffers();
    }

    void Renderer::resizeWindow(vk::Extent2D const &extents)
    {
        // Tearing down the FrameStatePool waits for GPU completion.
        m_framePool.reset();

        if (m_presenter)
        {
            m_presenter->clearFramebuffers();
        }
        m_depthBuffer = DepthBufferWrapper();
        m_renderPass = nullptr;

        // The old presenter is kept alive until its replacement exists so its swapchain can be handed over.
        // Assigning the new one then destroys the old one, including its retired swapchain.
        m_presenter = std::make_unique<Presenter>(
            m_foundry,
            extents,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            m_framesInFlight,
            m_presenter.get());

        // The surface may constrain the swapchain extent, so everything sized to the swapchain
        // (depth buffer, framebuffers, viewport, scissor) uses the extent it was actually created with.
        m_extents = m_presenter->extent();

        if (m_useDepthBuffer)
        {
            m_depthBuffer = DepthBufferWrapper(m_foundry, vk::Format::eD16Unorm, m_extents);
        }

        m_renderPass = createRenderPass(
            m_foundry->device(),
            m_presenter->colorFormat(),
            m_useDepthBuffer ? m_depthBuffer.format() : vk::Format::eUndefined);

        // Frames are allocated by the next renderFrame, so a derived constructor can set m_createFrameFunc first.
        m_presenter->createFramebuffers(m_renderPass, m_useDepthBuffer ? &m_depthBuffer.imageView() : nullptr);
    }

    void Renderer::waitIdle() const
    {
        m_foundry->waitIdle();
    }

    vk::Result Renderer::renderFrame(std::chrono::microseconds frameTime)
    {
        m_frameTime = frameTime;

        if (!m_framePool)
        {
            // If this is the first frame or the window has been resized, allocate the frames states.
            m_framePool = std::make_unique<FrameStatePool>(m_foundry, m_framesInFlight, m_createFrameFunc);
        }

        FrameStateGuard frameGuard = m_framePool->acquireFrame();
		FrameState &frameState = frameGuard.get();
        vk::Result acquireResult = m_presenter->acquireFrame(frameState);

        // If image acquisition failed, skip rendering and presentation.
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            return acquireResult;
        }

        {
            // Begin the render pass.
            vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };

            vk::RenderPassBeginInfo renderPassBeginInfo(
                m_renderPass,
                m_presenter->framebuffer(frameState.imageIndex),
                vk::Rect2D(vk::Offset2D(0, 0), m_extents),
                clearValues);

            CommandBufferWrapper commandBuffer(
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

        vk::Result result = m_presenter->presentFrame(frameState);

        m_frameCount++;

        return result;
    }
} // namespace spock

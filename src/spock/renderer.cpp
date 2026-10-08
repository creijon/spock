// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "renderer.hpp"

#include "creators.hpp"
#include "presenter.hpp"
#include "helpers.hpp"

namespace spock
{
    Renderer::Renderer(
        std::shared_ptr<const Foundry> const &foundry,
        vk::Extent2D const &extents,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer,
        FrameStatePool::CreateFrameFunc const &createFrameFunc)
        : m_foundry(foundry)
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
    {
        m_framePool = std::make_unique<FrameStatePool>(foundry, createFrameFunc);
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
        m_inFlightIndex = 0;

        // Reset waits for GPU completion before any render targets are destroyed.
        m_framePool->reset();
        if (m_presenter) m_presenter->clearFramebuffers();
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

        m_framePool->allocateFrames(m_framesInFlight);
        m_presenter->createFramebuffers(m_renderPass, m_useDepthBuffer ? &m_depthBuffer.imageView() : nullptr);
    }

    void Renderer::waitIdle() const
    {
        m_foundry->waitIdle();
    }

    vk::Result Renderer::renderFrame()
    {
        FrameStateGuard frameState = m_framePool->acquireFrame();
        vk::Result acquireResult = m_presenter->acquireFrame(frameState.get());

        // If image acquisition failed, return the unused frame and skip rendering and presentation.
        // The semaphore was not signaled by the swapchain, so we cannot wait on it.
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            return acquireResult;
        }

        // Begin the render pass.
        auto& commandBuffer = frameState.get().commandBuffer;

        commandBuffer.begin({});

        vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };

        vk::RenderPassBeginInfo renderPassBeginInfo(
            m_renderPass,
            m_presenter->framebuffer(frameState.get().imageIndex),
            vk::Rect2D(vk::Offset2D(0, 0), m_extents),
            clearValues);
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        // Setup the viewport and scissor rectangle.
        commandBuffer.setViewport(
            0, vk::Viewport(0.0f,
                0.0f,
                static_cast<float>(m_extents.width),
                static_cast<float>(m_extents.height),
                0.0f,
                1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_extents));

        // The derived class renders its scene into the command buffer.
        render(frameState.get());

        // End the render pass and submit the command buffer.
        commandBuffer.endRenderPass();
        commandBuffer.end();

        m_presenter->submitCommands(frameState.get());

        vk::Result result = m_presenter->presentFrame(frameState.get());

        m_inFlightIndex = (m_inFlightIndex + 1) % m_framesInFlight;
        m_frameCount++;

        return result;
    }
} // namespace spock

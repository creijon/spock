// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "renderer.hpp"

#include "creators.hpp"
#include "wrappers.hpp"

#include <chrono>
#include <iostream>

namespace spock
{
    Renderer::Renderer(
        vk::raii::Instance const &instance,
        vk::SurfaceKHR const& windowSurface,
        vk::Extent2D const &extents,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer,
        uint32_t framesInFlight)
        : m_physicalDevice(vk::raii::PhysicalDevices(instance).front())
        , m_windowSurface(instance, windowSurface)
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_framesInFlight(framesInFlight)
    {
        m_queueIndices = findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice, m_windowSurface);
        m_device = createDevice(m_physicalDevice, m_queueIndices.graphics, getDefaultDeviceExtensions());

        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_queueIndices.graphics};
        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        resizeWindow(extents);
    }

    void Renderer::resizeWindow(vk::Extent2D const &extents)
    {
        m_extents = extents;
        m_inFlightIndex = 0;

        // For resizing we need to clear out the previous framebuffers and command buffers before the swapchain.
        m_frameBuffers.clear();
        m_commandBuffers.clear();
        m_presenter.reset();

        m_presenter = std::make_unique<Presenter>(
            m_physicalDevice,
            m_device,
            m_windowSurface,
            m_extents,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            m_queueIndices,
            m_framesInFlight);

        vk::Format colorFormat = pickSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(m_windowSurface)).format;

        // Color and depth buffers, set up the render pass and framebuffers.
        if (m_useDepthBuffer)
        {
            m_depthBuffer = DepthBufferWrapper(m_physicalDevice, m_device, vk::Format::eD16Unorm, m_extents);
            m_renderPass = createRenderPass(m_device, colorFormat, m_depthBuffer.format());
            m_frameBuffers = createFramebuffers(
                m_device,
                m_renderPass,
                m_presenter->imageViews(),
                &m_depthBuffer.imageView(),
                m_extents);
        }
        else
        {
            m_renderPass = createRenderPass(m_device, colorFormat, vk::Format::eUndefined);
            m_frameBuffers = createFramebuffers(
                m_device,
                m_renderPass,
                m_presenter->imageViews(),
                nullptr,
                m_extents);
        }

        m_commandBuffers.reserve(m_framesInFlight);

        for (size_t i = 0; i < m_framesInFlight; i++)
        {
            m_commandBuffers.emplace_back(createCommandBuffer(m_device, m_commandPool));
        }
    }

    void Renderer::waitIdle() const
    {
        m_device.waitIdle();
    }

    vk::Result Renderer::renderFrame(std::chrono::microseconds time)
    {
        // Aquire the next frame in the swapchain and wait for the fence
        // to ensure that the previous frame has finished rendering.
        vk::Result acquireResult = m_presenter->acquireFrame(m_device, m_inFlightIndex);

        // Begin the render pass.
        auto& commandBuffer = m_commandBuffers[m_inFlightIndex];

        commandBuffer.begin({});

        vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };

        vk::RenderPassBeginInfo renderPassBeginInfo(
            m_renderPass,
            m_frameBuffers[m_presenter->imageIndex()],
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
        render(commandBuffer, time);

        // End the render pass and submit the command buffer.
        commandBuffer.endRenderPass();
        commandBuffer.end();

        m_presenter->submitCommands(commandBuffer, m_inFlightIndex);

        vk::Result result = m_presenter->presentFrame(m_inFlightIndex);

        m_inFlightIndex = (m_inFlightIndex + 1) % m_framesInFlight;
        m_frameCount++;

        return result;
    }
} // namespace spock

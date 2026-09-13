// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "renderer.hpp"

#include "creators.hpp"
#include "helpers.hpp"

#include <chrono>
#include <iostream>
#include <utility>

namespace spock
{
    Renderer::Renderer(
        vk::raii::Instance const &instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const &extents,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer,
        uint32_t framesInFlight)
        : m_physicalDevice(vk::raii::PhysicalDevices(instance).front())
        , m_windowSurface(std::move(windowSurface))
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_framesInFlight(framesInFlight)
    {
        m_queue = Queue(m_physicalDevice, m_windowSurface);
        m_device = createDevice(m_physicalDevice, m_queue.graphicsFamily(), getDefaultDeviceExtensions());

        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            m_queue.graphicsFamily()};
        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        resizeWindow(extents);
    }

    Renderer::~Renderer()
    {
        m_device.waitIdle();
    }

    void Renderer::resizeWindow(vk::Extent2D const &extents)
    {
        m_extents = extents;
        m_inFlightIndex = 0;
        m_framesSinceResize = 0;

        // For resizing we need to clear out the previous framebuffers and command buffers before the swapchain.
        m_renderPass = RenderPass();
        m_commandBuffers.clear();
        m_presenter.reset();

        m_presenter = std::make_unique<Presenter>(
            m_physicalDevice,
            m_device,
            m_windowSurface,
            m_extents,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            m_queue,
            m_framesInFlight);

        vk::Format colorFormat = pickSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(m_windowSurface)).format;

        m_renderPass = RenderPass(
            m_physicalDevice,
            m_device,
            m_presenter->imageViews(),
            colorFormat,
            m_extents,
            m_clearColor,
            m_clearDepthStencil,
            m_useDepthBuffer);

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
        vk::Result acquireResult = m_presenter->acquireFrame(m_device, m_inFlightIndex);

        // If frame acquisition failed, skip rendering and present this frame.
        // The semaphore was not signaled by the swapchain, so we cannot wait on it.
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            m_inFlightIndex = (m_inFlightIndex + 1) % m_framesInFlight;
            return acquireResult;
        }

        // Begin the render pass.
        auto& commandBuffer = m_commandBuffers[m_inFlightIndex];

        commandBuffer.begin({});

        m_renderPass.begin(commandBuffer, m_presenter->imageIndex(), m_extents);

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
        m_renderPass.end(commandBuffer);
        commandBuffer.end();

        m_presenter->submitCommands(commandBuffer, m_inFlightIndex);

        vk::Result result = m_presenter->presentFrame(m_inFlightIndex);

        m_inFlightIndex = (m_inFlightIndex + 1) % m_framesInFlight;
        m_frameCount++;
        m_framesSinceResize++;

        return result;
    }
} // namespace spock


#include "framework.hpp"

#include "creators.hpp"
#include "wrappers.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace spock
{
    // Initialize the framework, create the Vulkan instance, device,
    // surface, swap chain, depth buffer, render pass, framebuffers and the
    // per-frame command buffers and synchronization objects.
    Framework::Framework(
        char const* name,
        uint32_t windowWidth,
        uint32_t windowHeight,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer,
        uint32_t framesInFlight,
        std::chrono::microseconds frameDuration)
        : m_context()
        , m_instance(createInstance(m_context, name, {}, getInstanceExtensions()))
        , m_physicalDevice(vk::raii::PhysicalDevices(m_instance).front())
        , m_name(name)
        , m_extents(windowWidth, windowHeight)
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_framesInFlight(framesInFlight)
        , m_frameDuration(frameDuration)
    {
        m_handle = createWindow(m_name, m_extents);

        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(*m_instance, m_handle, nullptr, &surface);
        if (err != VK_SUCCESS)
            throw std::runtime_error("Failed to create window!");
        m_surface = vk::raii::SurfaceKHR(m_instance, surface);

        std::pair<uint32_t, uint32_t> familyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice, m_surface);
        m_device = createDevice(m_physicalDevice, familyIndex.first, getDeviceExtensions());

        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            familyIndex.first};

        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        resizeWindow(windowWidth, windowHeight);
    }

    void Framework::resizeWindow(uint32_t width, uint32_t height)
    {
        m_extents = vk::Extent2D(width, height);

        // For resizing we need to clear out the previous framebuffers and command buffers before the swapchain.
        m_frameBuffers.clear();
        m_commandBuffers.clear();
        m_presenter.reset();

        std::pair<uint32_t, uint32_t> familyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice, m_surface);

        m_presenter = std::make_unique<Presenter>(
            m_physicalDevice,
            m_device,
            m_surface,
            m_extents,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            familyIndex.first,
            familyIndex.second,
            m_framesInFlight);

        vk::Format colorFormat = pickSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(m_surface)).format;

        // Color and depth buffers, set up the render pass and framebuffers.
        if (m_useDepthBuffer)
        {
            m_depthBuffer = DepthBufferWrapper(m_physicalDevice, m_device, vk::Format::eD16Unorm, m_extents);
            m_renderPass = createRenderPass(m_device, colorFormat, m_depthBuffer.format);
            m_frameBuffers = createFramebuffers(m_device, m_renderPass, m_presenter->imageViews(), &m_depthBuffer.imageView, m_extents);
        }
        else
        {
            m_renderPass = createRenderPass(m_device, colorFormat, vk::Format::eUndefined);
            m_frameBuffers = createFramebuffers(m_device, m_renderPass, m_presenter->imageViews(), nullptr, m_extents);
        }

        m_commandBuffers.reserve(m_framesInFlight);

        for (size_t i = 0; i < m_framesInFlight; i++)
        {
            m_commandBuffers.emplace_back(createCommandBuffer(m_device, m_commandPool));
        }
    }

    void Framework::run()
    {
        uint32_t inFlightIndex = 0;
        auto startTime{std::chrono::steady_clock::now()};
        m_time = std::chrono::microseconds(0);

        while (!glfwWindowShouldClose(m_handle))
        {
            glfwPollEvents();

            update();

            // Aquire the next frame in the swapchain and wait for the fence
            // to ensure that the previous frame has finished rendering.
            m_presenter->acquireFrame(m_device);

            // Begin the render pass.
            auto& commandBuffer = m_commandBuffers[inFlightIndex];

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
            render(commandBuffer);

            // End the render pass and submit the command buffer.
            commandBuffer.endRenderPass();
            commandBuffer.end();

            m_presenter->submitCommands(commandBuffer);

            vk::Result result = m_presenter->presentFrame();

            // Handle window resizing.
            if (result == vk::Result::eSuboptimalKHR || !m_presenter->isValid())
            {
                int width, height;
                glfwGetFramebufferSize(m_handle, &width, &height);
                
                m_device.waitIdle();

                resizeWindow(width, height);
            }

            m_time += m_frameDuration;
            inFlightIndex = (inFlightIndex + 1) % m_framesInFlight;
            m_frameCount++;

            std::this_thread::sleep_until(startTime + m_time);
        }

        m_device.waitIdle();
    }
} // namespace spock


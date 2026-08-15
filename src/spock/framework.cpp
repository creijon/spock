
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
        , m_useDepthBuffer(useDepthBuffer)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_framesInFlight(framesInFlight)
        , m_frameDuration(frameDuration)
    {
        resizeWindow(name, windowWidth, windowHeight);

        std::pair<uint32_t, uint32_t> familyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice, m_surface);
        m_device = createDevice(m_physicalDevice, familyIndex.first, getDeviceExtensions());

        vk::CommandPoolCreateInfo poolInfo{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient,
            familyIndex.first};

        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

        rebuildSwapchain();

        m_commandBuffers.reserve(m_framesInFlight);

        for (size_t i = 0; i < m_framesInFlight; i++)
        {
            m_commandBuffers.emplace_back(createCommandBuffer(m_device, m_commandPool));
        }
    }

    void Framework::resizeWindow(char const* name, uint32_t windowWidth, uint32_t windowHeight)
    {
        m_name = name;
        m_extents = vk::Extent2D(windowWidth, windowHeight);
        m_handle = createWindow(name, m_extents);

        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(*m_instance, m_handle, nullptr, &surface);
        if (err != VK_SUCCESS)
            throw std::runtime_error("Failed to create window!");
        m_surface = vk::raii::SurfaceKHR(m_instance, surface);
    }

    void Framework::rebuildSwapchain()
    {
        std::pair<uint32_t, uint32_t> familyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice, m_surface);

        if (!m_presenter) m_presenter = std::make_unique<Presenter>();

        m_presenter->initialise(
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
            
            m_presenter->acquireFrame(m_device);

            // Begin the render pass.
            auto& commandBuffer = m_commandBuffers[inFlightIndex];

            commandBuffer.begin({});

            std::array<vk::ClearValue, 2> clearValues;
            clearValues[0].color = m_clearColor;
            clearValues[1].depthStencil = m_clearDepthStencil;

            vk::RenderPassBeginInfo renderPassBeginInfo(
                m_renderPass, m_frameBuffers[m_presenter->imageIndex()], vk::Rect2D(vk::Offset2D(0, 0), m_extents), clearValues);
            commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            // Setup the viewport and scissor.
            commandBuffer.setViewport(
                0, vk::Viewport(0.0f, 0.0f, static_cast<float>(m_extents.width), static_cast<float>(m_extents.height), 0.0f, 1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_extents));

            // The derived class renders its scene.
            render(commandBuffer);

            // End the render pass and submit the command buffer.
            commandBuffer.endRenderPass();
            commandBuffer.end();

            vk::Result result = m_presenter->presentFrame(commandBuffer);

            if (result == vk::Result::eSuboptimalKHR || !m_presenter->isValid())
            {
                // TODO: rewrite this to use the resizeWindow() function
                // Wait for all work to be finished.
                m_device.waitIdle();

                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(m_handle, &width, &height);

                m_extents.setWidth(width);
                m_extents.setHeight(height);

                for (size_t i = 0; i < m_framesInFlight; i++) {
                    m_commandBuffers[i] = createCommandBuffer(m_device, m_commandPool);
                }

                m_presenter.reset();

                VkSurfaceKHR surface;
                VkResult err = glfwCreateWindowSurface(*m_instance, m_handle, nullptr, &surface);
                if (err != VK_SUCCESS)
                    throw std::runtime_error("Failed to create window!");

                m_surface = vk::raii::SurfaceKHR(m_instance, surface);

                rebuildSwapchain();
            }

            m_time += m_frameDuration;
            inFlightIndex = (inFlightIndex + 1) % m_framesInFlight;
            m_frameCount++;

            std::this_thread::sleep_until(startTime + m_time);
        }

        m_device.waitIdle();
    }
} // namespace spock


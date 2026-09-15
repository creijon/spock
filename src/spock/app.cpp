// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "app.hpp"

#include "creators.hpp"
#include "renderer.hpp"

#include <thread>
#include <utility>

namespace
{
    std::vector<std::string> getDefaultInstanceExtensions()
    {
        std::vector<std::string> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_VI_NN)
        extensions.push_back(VK_NN_VI_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
        extensions.push_back(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
#endif
        return extensions;
    }
}

namespace spock
{

App::App(
    char const* name,
    uint32_t windowWidth,
    uint32_t windowHeight,
    std::chrono::microseconds frameDuration)
    : m_context()
    , m_instance(createInstance(m_context, name, {}, getDefaultInstanceExtensions()))
    , m_window(name, vk::Extent2D(windowWidth, windowHeight))
    , m_frameDuration(frameDuration)
{
}

void App::run()
{
    vk::raii::SurfaceKHR windowSurface = m_window.createSurface(m_instance);

    m_renderer = createRenderer(m_instance, std::move(windowSurface), m_window.extents());

    auto startTime{std::chrono::steady_clock::now()};
    m_time = std::chrono::microseconds(0);

    while (!m_window.shouldClose())
    {
        Window::pollEvents();

        // Update and render frame.
        update();
        vk::Result result = m_renderer->renderFrame(m_time);

        // Check for window resize.
        vk::Extent2D fbExtents = m_window.framebufferSize();

        const bool sizeChanged = (fbExtents != m_window.extents());

        if (sizeChanged ||
            result == vk::Result::eSuboptimalKHR ||
            result == vk::Result::eErrorOutOfDateKHR)
        {
            // Ignore zero-sized framebuffers (minimized / hidden on some platforms).
            if (fbExtents.width == 0 || fbExtents.height == 0)
            {
                continue;
            }

            m_window.setExtents(fbExtents);
            m_renderer->waitIdle();
            m_renderer->resizeWindow(fbExtents);
        }

        m_time += m_frameDuration;

        std::this_thread::sleep_until(startTime + m_time);
    }

    m_renderer->waitIdle();
}

} // namespace spock

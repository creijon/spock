#include "app.hpp"

#include "creators.hpp"
#include "renderer.hpp"

#include <thread>
#include <stdexcept>

namespace spock
{

App::App(
    char const* name,
    uint32_t windowWidth,
    uint32_t windowHeight,
    std::chrono::microseconds frameDuration)
    : m_name(name)
    , m_extents(windowWidth, windowHeight)
    , m_context()
    , m_instance(createInstance(m_context, m_name, {}, getDefaultInstanceExtensions()))
    , m_frameDuration(frameDuration)
{
    m_windowHandle = createWindow(m_name, m_extents);
}

App::~App()
{
    if (m_renderer)
    {
        m_renderer->waitIdle();
    }
}

void App::run()
{
    VkSurfaceKHR windowSurface;
    VkResult err = glfwCreateWindowSurface(*m_instance, m_windowHandle, nullptr, &windowSurface);

    m_renderer = createRenderer(m_instance, windowSurface, m_extents);

    auto startTime{std::chrono::steady_clock::now()};
    m_time = std::chrono::microseconds(0);

    while (!glfwWindowShouldClose(m_windowHandle))
    {
        glfwPollEvents();

        // Update and render frame.
        update();
        vk::Result result = m_renderer->renderFrame(m_time);

        // Check for window resize.
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_windowHandle, &fbWidth, &fbHeight);
        
        const bool sizeChanged = (fbWidth != int(m_extents.width) || fbHeight != int(m_extents.height));

        if (sizeChanged ||
            result == vk::Result::eSuboptimalKHR ||
            result == vk::Result::eErrorOutOfDateKHR)
        {
            // Ignore zero-sized framebuffers (minimized / hidden on some platforms).
            if (fbWidth == 0 || fbHeight == 0)
            {
                continue;
            }

            m_extents = vk::Extent2D(fbWidth, fbHeight);
            m_renderer->waitIdle();
            m_renderer->resizeWindow(m_extents);
        }

        m_time += m_frameDuration;

        std::this_thread::sleep_until(startTime + m_time);
    }
}

} // namespace spock

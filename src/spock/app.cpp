#include "app.hpp"

#include "creators.hpp"
#include "renderer.hpp"

#include <thread>

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
    vk::SurfaceKHR windowSurface = m_window.createSurface(m_instance);

    m_renderer = createRenderer(m_instance, windowSurface, m_window.extents());

    auto startTime{std::chrono::steady_clock::now()};
    m_time = std::chrono::microseconds(0);

    while (!m_window.shouldClose())
    {
        glfwPollEvents();

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
}

} // namespace spock

#include "window.hpp"

#include "creators.hpp"

namespace spock
{

Window::Window(
    std::string const &name,
    vk::Extent2D const &extents)
    : m_name(name)
    , m_extents(extents)
    , m_handle(createWindow(m_name, m_extents))
{
}

Window::~Window()
{
    glfwDestroyWindow(m_handle);
}

vk::SurfaceKHR Window::createSurface(vk::raii::Instance const &instance) const
{
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(*instance, m_handle, nullptr, &surface);
    return surface;
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_handle);
}

vk::Extent2D Window::framebufferSize() const
{
    int width, height;
    glfwGetFramebufferSize(m_handle, &width, &height);
    return vk::Extent2D(width, height);
}

} // namespace spock

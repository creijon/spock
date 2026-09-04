#include "window.hpp"

#include <GLFW/glfw3.h>

#include <iostream>

namespace spock
{
namespace
{
    int toGlfwButton(MouseButton button)
    {
        switch (button)
        {
        case MouseButton::Left:
            return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Right:
            return GLFW_MOUSE_BUTTON_RIGHT;
        case MouseButton::Middle:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        }
        return GLFW_MOUSE_BUTTON_LEFT;
    }

    GLFWwindow* createGlfwWindow(
        std::string const &windowName,
        vk::Extent2D const &extent)
    {
        struct glfwContext
        {
            glfwContext()
            {
                glfwInit();
                glfwSetErrorCallback([](int error, const char* msg)
                    { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; });
            }

            ~glfwContext()
            {
                glfwTerminate();
            }
        };

        static auto glfwCtx = glfwContext();
        (void)glfwCtx;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        return glfwCreateWindow(extent.width, extent.height, windowName.c_str(), nullptr, nullptr);
    }

    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        Window* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        win->setScrollWheelOffset(xoffset, yoffset);
    }
} // namespace

Window::Window(
    std::string const &name,
    vk::Extent2D const &extents)
    : m_name(name)
    , m_extents(extents)
    , m_handle(createGlfwWindow(m_name, m_extents))
{
    glfwSetWindowUserPointer(m_handle, this);
    glfwSetScrollCallback(m_handle, scrollCallback);
}

Window::~Window()
{
    glfwDestroyWindow(m_handle);
}

vk::raii::SurfaceKHR Window::createSurface(vk::raii::Instance const &instance) const
{
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(*instance, m_handle, nullptr, &surface);
    return vk::raii::SurfaceKHR(instance, surface);
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

vk::Offset2D Window::cursorPosition() const
{
    double x, y;
    glfwGetCursorPos(m_handle, &x, &y);

    return vk::Offset2D(int32_t(x), int32_t(y));
}

bool Window::isMouseButtonPressed(MouseButton button) const
{
    return glfwGetMouseButton(m_handle, toGlfwButton(button)) == GLFW_PRESS;
}

void Window::setScrollWheelOffset(double xoffset, double yoffset)
{
    m_scrollWheelX = xoffset;
    m_scrollWheelY = yoffset;
}

void Window::pollEvents()
{
    glfwPollEvents();
}

} // namespace spock

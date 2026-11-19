// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/window.hpp"

// Keeps GLFW from including the OpenGL headers.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

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
        GLFWwindow* window = glfwCreateWindow(extent.width, extent.height, windowName.c_str(), nullptr, nullptr);

        if (window == nullptr)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }

        return window;
    }

    vk::raii::SurfaceKHR createGlfwSurface(vk::raii::Instance const &instance, GLFWwindow* window)
    {
        VkSurfaceKHR surface;
        VkResult result = glfwCreateWindowSurface(*instance, window, nullptr, &surface);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface");
        }
        return vk::raii::SurfaceKHR(instance, surface);
    }

    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        Window* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        win->setScrollWheelOffset(xoffset, yoffset);
    }
} // namespace

Window::Window(
    std::string const &name,
    vk::Extent2D const &extents,
    vk::raii::Instance const &instance)
    : m_name(name)
    , m_extents(extents)
    , m_handle(createGlfwWindow(m_name, m_extents))
{
    try
    {
        m_surface = createGlfwSurface(instance, m_handle);
    }
    catch (...)
    {
        // The destructor doesn't run when the constructor throws.
        glfwDestroyWindow(m_handle);
        throw;
    }

    glfwSetWindowUserPointer(m_handle, this);
    glfwSetScrollCallback(m_handle, scrollCallback);
}

Window::~Window()
{
    // The surface refers to the native window, so it goes first.
    m_surface.clear();
    glfwDestroyWindow(m_handle);
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

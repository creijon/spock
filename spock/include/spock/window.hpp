// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>

typedef struct GLFWwindow GLFWwindow;

namespace spock
{
    enum class MouseButton
    {
        Left,
        Right,
        Middle,
    };

    // Owns the platform window, its Vulkan surface and its input state. The underlying windowing
    // library (currently GLFW) is an implementation detail confined to window.cpp.
    class Window
    {
    public:
        // The instance must outlive the Window, because the Window owns a surface created from it.
        Window(
            std::string const &name,
            vk::Extent2D const &extents,
            vk::raii::Instance const &instance);

        Window(const Window &) = delete;

        ~Window();

        // The surface lives exactly as long as the Window, so anything built on it, such as a
        // swapchain, must be destroyed first.
        vk::SurfaceKHR surface() const { return *m_surface; }

        bool shouldClose() const;
        vk::Extent2D framebufferSize() const;

        vk::Offset2D cursorPosition() const;
        bool isMouseButtonPressed(MouseButton button) const;
        double scrollWheelOffsetX() const
        {
            return m_scrollWheelX;
        }
        double scrollWheelOffsetY() const
        {
            return m_scrollWheelY;
        }
        void setScrollWheelOffset(double xoffset, double yoffset);

        std::string const& name() const { return m_name; }

        vk::Extent2D const& extents() const { return m_extents; }
        void setExtents(vk::Extent2D const &extents) { m_extents = extents; }

        // Polls and dispatches pending events for all windows.
        static void pollEvents();

    private:
        std::string m_name;
        vk::Extent2D m_extents;
        GLFWwindow *m_handle{nullptr};
        // Destroyed explicitly before m_handle, because it refers to the native window.
        vk::raii::SurfaceKHR m_surface{nullptr};
        double m_scrollWheelX{0.0};
        double m_scrollWheelY{0.0};

    };
} // namespace spock

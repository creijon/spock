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

    // Owns the platform window and its input state. The underlying windowing
    // library (currently GLFW) is an implementation detail confined to window.cpp.
    class Window
    {
    public:
        Window(
            std::string const &name,
            vk::Extent2D const &extents);

        Window(const Window &) = delete;

        ~Window();

        vk::raii::SurfaceKHR createSurface(vk::raii::Instance const &instance) const;

        bool shouldClose() const;
        vk::Extent2D framebufferSize() const;

        vk::Offset2D cursorPosition() const;
        bool isMouseButtonPressed(MouseButton button) const;

        std::string const& name() const { return m_name; }

        vk::Extent2D const& extents() const { return m_extents; }
        void setExtents(vk::Extent2D const &extents) { m_extents = extents; }

        // Polls and dispatches pending events for all windows.
        static void pollEvents();

    private:
        std::string m_name;
        vk::Extent2D m_extents;

        GLFWwindow *m_handle{nullptr};
    };
} // namespace spock

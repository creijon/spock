// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>

typedef struct GLFWwindow GLFWwindow;

namespace spock
{
    // Owns the GLFW window handle.
    class Window
    {
    public:
        Window(
            std::string const &name,
            vk::Extent2D const &extents);

        Window(const Window &) = delete;

        ~Window();

        vk::SurfaceKHR createSurface(vk::raii::Instance const &instance) const;

        bool shouldClose() const;
        vk::Extent2D framebufferSize() const;

        std::string const& name() const { return m_name; }

        vk::Extent2D const& extents() const { return m_extents; }
        void setExtents(vk::Extent2D const &extents) { m_extents = extents; }

        GLFWwindow* handle() const { return m_handle; }

    private:
        std::string m_name;
        vk::Extent2D m_extents;

        GLFWwindow *m_handle{nullptr};
    };
} // namespace spock

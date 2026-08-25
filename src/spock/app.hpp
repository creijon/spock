// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <memory>
#include <string>

using namespace std::chrono_literals;

typedef struct GLFWwindow GLFWwindow;

namespace spock
{
    class Renderer;

    class App
    {
    public:
        App(
            char const* name,
            uint32_t windowWidth,
            uint32_t windowHeight,
            std::chrono::microseconds frameDuration = 16666us);

        virtual ~App();

        void run();

    protected:
        virtual std::unique_ptr<spock::Renderer> createRenderer(
            vk::raii::Instance const& instance,
            vk::SurfaceKHR const& windowSurface,
            vk::Extent2D const& extents) = 0;

        // Called once per frame before rendering.
        virtual void update() = 0;

        std::string m_name;
        vk::Extent2D m_extents;

        vk::raii::Context m_context{};
        vk::raii::Instance m_instance{nullptr};

        GLFWwindow *m_windowHandle{nullptr};

        std::unique_ptr<Renderer> m_renderer;

        std::chrono::microseconds m_time{};

        const std::chrono::microseconds m_frameDuration;
    };
} // namespace spock

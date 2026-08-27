// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "window.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <utility>

using namespace std::chrono_literals;

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

        virtual ~App() = default;

        void run();

    protected:
        virtual std::unique_ptr<spock::Renderer> createRenderer(
            vk::raii::Instance const& instance,
            vk::raii::SurfaceKHR windowSurface,
            vk::Extent2D const& extents) = 0;

        // Called once per frame before rendering.
        virtual void update() = 0;

        vk::raii::Context m_context{};
        vk::raii::Instance m_instance{nullptr};

        Window m_window;

        std::unique_ptr<Renderer> m_renderer;

        std::chrono::microseconds m_time{};

        const std::chrono::microseconds m_frameDuration;
    };

    // Constructs an AppT with the given arguments, runs it, and reports any
    // exception that escapes the main loop. Returns the process exit code.
    template <typename AppT, typename... Args>
    int runApp(Args&&... args)
    {
        try
        {
            AppT app(std::forward<Args>(args)...);
            app.run();
        }
        catch (vk::SystemError &err)
        {
            std::cout << "vk::SystemError: " << err.what() << std::endl;
            return -1;
        }
        catch (std::exception &err)
        {
            std::cout << "std::exception: " << err.what() << std::endl;
            return -1;
        }
        catch (...)
        {
            std::cout << "unknown error\n";
            return -1;
        }
        return 0;
    }
} // namespace spock

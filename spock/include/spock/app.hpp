// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "controller.hpp"
#include "types.hpp"
#include "window.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <memory>
#include <utility>

using namespace std::chrono_literals;

namespace spock
{
    class App
    {
    public:
        App(
            char const* name,
            uint32_t windowWidth,
            uint32_t windowHeight,
            std::chrono::microseconds frameDuration = 16666us,
            uint32_t apiVersion = VK_API_VERSION_1_2);

        virtual ~App() = default;

        void run();

    protected:
        virtual FoundryPtr createFoundry() const;

        // Called by the Controller, which picks the color format and the number of frames in flight.
        virtual std::unique_ptr<Renderer> createRenderer(vk::Format colorFormat, uint32_t framesInFlight) = 0;

        // Called once per frame before rendering.
        virtual void update() = 0;

        template <typename T>
        T &renderer()
        {
            return static_cast<T &>(m_controller->renderer());
        }

        // Members are destroyed in reverse order, which matters here: the controller's swapchain must go
        // before the window's surface, and the surface before the instance.
        vk::raii::Context m_context{};
        vk::raii::Instance m_instance{nullptr};

        Window m_window;
        FoundryPtr m_foundry;
        std::unique_ptr<Controller> m_controller;

        std::chrono::microseconds m_time{};

        const std::chrono::microseconds m_frameDuration;
    };

    // Reports the exception currently being handled and returns the process exit code.
    int reportUncaughtException();

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
        catch (...)
        {
            return reportUncaughtException();
        }
        return 0;
    }
} // namespace spock

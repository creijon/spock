// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"
#include "window.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <memory>
#include <utility>

using namespace std::chrono_literals;

namespace spock
{
    class Renderer;

    class App
    {
    public:
        // apiVersion: the Vulkan version the instance is created with, which caps the core functions the
        // device exposes.  Raise it for an app that uses something newer than 1.2, such as FFmpeg's Vulkan decoder.
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
        virtual std::unique_ptr<Renderer> createRenderer() = 0;

        // Called once per frame before rendering.
        virtual void update() = 0;

        vk::raii::Context m_context{};
        vk::raii::Instance m_instance{nullptr};

        Window m_window;
        FoundryPtr m_foundry;
        std::unique_ptr<Renderer> m_renderer;

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

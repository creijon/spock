// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "spock/creators.hpp"
#include "spock/foundry.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <iostream>
#include <memory>

namespace spock_test
{
    // A real, headless (no window/display) Vulkan instance + Foundry pair,
    // built with VK_EXT_headless_surface so device-backed pieces of the spock
    // API can be exercised without GLFW or a windowing system. The Foundry
    // owns the physical device, surface, logical device, queues and command
    // pool, exactly as it does in a running App. Tests that use this should
    // be tagged "[gpu]" and skip gracefully via createGpuFixture() returning
    // nullptr when no usable Vulkan driver is present.
    struct GpuFixture
    {
        vk::raii::Context context;
        vk::raii::Instance instance{nullptr};
        std::shared_ptr<spock::Foundry> foundry;
    };

    inline std::unique_ptr<GpuFixture> createGpuFixture()
    {
        auto fixture = std::make_unique<GpuFixture>();
        try
        {
            fixture->instance = spock::createInstance(
                fixture->context,
                "spock-tests",
                {},
                {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME});

            fixture->foundry = std::make_shared<spock::Foundry>(
                fixture->instance,
                vk::raii::SurfaceKHR(fixture->instance, vk::HeadlessSurfaceCreateInfoEXT{}));
        }
        catch (std::exception const &e)
        {
            std::cerr << "spock tests: no usable headless Vulkan device available (" << e.what() << "); skipping [gpu] test\n";
            return nullptr;
        }
        return fixture;
    }
} // namespace spock_test

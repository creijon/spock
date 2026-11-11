// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "spock/creators.hpp"
#include "spock/foundry.hpp"
#include "spock/types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

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
        spock::FoundryPtr foundry;
    };

    inline std::unique_ptr<GpuFixture> createGpuFixture()
    {
        auto fixture = std::make_unique<GpuFixture>();
        try
        {
            std::vector<std::string> extensions{
                VK_KHR_SURFACE_EXTENSION_NAME,
                VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME};
#if defined(__APPLE__)
            // createInstance sets the portability enumeration flag on Apple, which the loader only honours
            // when the matching extension is enabled. Without it MoltenVK is hidden and instance creation
            // fails with ErrorIncompatibleDriver, silently skipping every [gpu] test.
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

            fixture->instance = spock::createInstance(
                fixture->context,
                "spock-tests",
                {},
                extensions);

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

    // The FIFO, FIFO relaxed, mailbox and immediate present modes the Foundry's surface supports. The shared
    // present modes are left out because they need a presentable image of their own.
    inline std::vector<vk::PresentModeKHR> supportedPresentModes(spock::Foundry const &foundry)
    {
        std::vector<vk::PresentModeKHR> modes;
        for (vk::PresentModeKHR mode : foundry.physicalDevice().getSurfacePresentModesKHR(*foundry.surface()))
        {
            if (mode == vk::PresentModeKHR::eFifo || mode == vk::PresentModeKHR::eFifoRelaxed ||
                mode == vk::PresentModeKHR::eMailbox || mode == vk::PresentModeKHR::eImmediate)
            {
                modes.push_back(mode);
            }
        }
        return modes;
    }

    // Each composite alpha mode the Foundry's surface supports.
    inline std::vector<vk::CompositeAlphaFlagBitsKHR> supportedCompositeAlpha(spock::Foundry const &foundry)
    {
        vk::CompositeAlphaFlagsKHR const supported =
            foundry.physicalDevice().getSurfaceCapabilitiesKHR(*foundry.surface()).supportedCompositeAlpha;

        std::vector<vk::CompositeAlphaFlagBitsKHR> modes;
        for (vk::CompositeAlphaFlagBitsKHR mode : {
                 vk::CompositeAlphaFlagBitsKHR::eOpaque,
                 vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                 vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
                 vk::CompositeAlphaFlagBitsKHR::eInherit})
        {
            if (supported & mode)
            {
                modes.push_back(mode);
            }
        }
        return modes;
    }
} // namespace spock_test

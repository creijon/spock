#pragma once

#include "spock/creators.hpp"
#include "spock/helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <iostream>
#include <memory>

namespace spock_test
{
    // A real, headless (no window/display) Vulkan instance + device pair,
    // built with VK_EXT_headless_surface so device-backed pieces of the spock
    // API can be exercised without GLFW or a windowing system. Tests that use
    // this should be tagged "[gpu]" and skip gracefully via createGpuFixture()
    // returning nullptr when no usable Vulkan driver is present.
    struct GpuFixture
    {
        vk::raii::Context context;
        vk::raii::Instance instance{nullptr};
        vk::raii::PhysicalDevice physicalDevice{nullptr};
        vk::raii::SurfaceKHR surface{nullptr};
        spock::QueueIndices queueIndices{};
        vk::raii::Device device{nullptr};
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

            fixture->physicalDevice = vk::raii::PhysicalDevices(fixture->instance).front();

            fixture->surface = vk::raii::SurfaceKHR(fixture->instance, vk::HeadlessSurfaceCreateInfoEXT{});

            fixture->queueIndices = spock::findGraphicsAndPresentQueueFamilyIndex(fixture->physicalDevice, fixture->surface);

            fixture->device = spock::createDevice(
                fixture->physicalDevice,
                fixture->queueIndices.graphics,
                spock::getDefaultDeviceExtensions());
        }
        catch (std::exception const &e)
        {
            std::cerr << "spock tests: no usable headless Vulkan device available (" << e.what() << "); skipping [gpu] test\n";
            return nullptr;
        }
        return fixture;
    }
} // namespace spock_test

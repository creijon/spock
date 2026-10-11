// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"

#include "spock/device_selector.hpp"
#include "spock/types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

using namespace spock_test;

// These helpers are implemented in device_selector.cpp as ordinary (non-static) free
// functions but are intentionally not exposed via device_selector.hpp since they're
// implementation details of the queue family discovery. They operate purely on
// plain data, so they're worth unit-testing directly via a matching forward
// declaration -- same convention used for findMemoryType in helpers_tests.cpp.
namespace spock
{
    std::optional<uint32_t> findQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);

    std::optional<uint32_t> findQueueFamilyIndexFallback(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);
} // namespace spock

namespace
{
    bool hasFlags(std::vector<vk::QueueFamilyProperties> const &families, uint32_t index, vk::QueueFlags flags)
    {
        return index < families.size() && (families[index].queueFlags & flags) == flags;
    }

    // Transfer capability is implied by graphics or compute, so a family qualifies for transfer
    // work if it carries any of the three bits.
    bool supportsTransfer(std::vector<vk::QueueFamilyProperties> const &families, uint32_t index)
    {
        return hasFlags(families, index, vk::QueueFlagBits::eTransfer) ||
               hasFlags(families, index, vk::QueueFlagBits::eGraphics) ||
               hasFlags(families, index, vk::QueueFlagBits::eCompute);
    }

    vk::raii::SurfaceKHR createHeadlessSurface(GpuFixture const &fixture)
    {
        return vk::raii::SurfaceKHR(fixture.instance, vk::HeadlessSurfaceCreateInfoEXT{});
    }
} // namespace

// ---------------------------------------------------------------------------
// Pure-logic queue family helpers (no GPU required).
// ---------------------------------------------------------------------------

TEST_CASE("findQueueFamilyIndex returns the first family matching the required flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[2].queueFlags = vk::QueueFlagBits::eGraphics;

    auto index = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eGraphics, {});
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndex requires every included flag to be present", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;
    families[2].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, {});
    REQUIRE(index.has_value());
    CHECK(*index == 2);
}

TEST_CASE("findQueueFamilyIndex excludes families that carry an excluded flag", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndex excludes a family that carries any one of several excluded flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
    families[1].queueFlags = vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
    families[2].queueFlags = vk::QueueFlagBits::eTransfer;

    auto index = spock::findQueueFamilyIndex(
        families,
        vk::QueueFlagBits::eTransfer,
        vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
    REQUIRE(index.has_value());
    CHECK(*index == 2);
}

TEST_CASE("findQueueFamilyIndex returns an empty optional when nothing matches", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK_FALSE(spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eCompute, {}).has_value());
}

TEST_CASE("findQueueFamilyIndex returns an empty optional for an empty family list", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families;

    CHECK_FALSE(spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eGraphics, {}).has_value());
}

TEST_CASE("findQueueFamilyIndexFallback prefers a family that excludes the given flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndexFallback falls back to a family that carries the excluded flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 0);
}

TEST_CASE("findQueueFamilyIndexFallback returns an empty optional when nothing supports the flag at all", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK_FALSE(spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, {}).has_value());
}

// ---------------------------------------------------------------------------
// selectDevice against a real, headless Vulkan instance.
// ---------------------------------------------------------------------------

TEST_CASE("selectDevice picks a device whose queue families satisfy graphics and present", "[gpu][device]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device available");

    vk::raii::SurfaceKHR surface = createHeadlessSurface(*fixture);

    auto [physicalDevice, queueFamilies] = spock::selectDevice(fixture->instance, *surface);
    CHECK(*physicalDevice != VK_NULL_HANDLE);

    auto families = physicalDevice.getQueueFamilyProperties();
    CHECK(hasFlags(families, queueFamilies.graphics, vk::QueueFlagBits::eGraphics));
    CHECK(queueFamilies.present < families.size());
    CHECK(physicalDevice.getSurfaceSupportKHR(queueFamilies.present, *surface));

    // Compute and transfer default to the graphics family when they are not requested.
    CHECK(queueFamilies.compute == queueFamilies.graphics);
    CHECK(queueFamilies.transfer == queueFamilies.graphics);
}

TEST_CASE("selectDevice honours a request for compute and transfer queues", "[gpu][device]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device available");

    vk::raii::SurfaceKHR surface = createHeadlessSurface(*fixture);

    auto [physicalDevice, queueFamilies] = spock::selectDevice(
        fixture->instance,
        *surface,
        vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer);

    auto families = physicalDevice.getQueueFamilyProperties();
    CHECK(hasFlags(families, queueFamilies.graphics, vk::QueueFlagBits::eGraphics));
    CHECK(hasFlags(families, queueFamilies.compute, vk::QueueFlagBits::eCompute));
    CHECK(supportsTransfer(families, queueFamilies.transfer));

    // When the device offers a compute family without graphics, it must be preferred over
    // sharing the graphics family.
    auto dedicatedCompute = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    if (dedicatedCompute.has_value())
    {
        CHECK_FALSE(hasFlags(families, queueFamilies.compute, vk::QueueFlagBits::eGraphics));
    }
    else
    {
        CHECK(queueFamilies.compute == queueFamilies.graphics);
    }

    // Likewise a transfer-only family must be preferred when one exists.
    auto dedicatedTransfer = spock::findQueueFamilyIndex(
        families,
        vk::QueueFlagBits::eTransfer,
        vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
    if (dedicatedTransfer.has_value())
    {
        CHECK(queueFamilies.transfer == *dedicatedTransfer);
    }
    else
    {
        CHECK(queueFamilies.transfer == queueFamilies.graphics);
    }
}

TEST_CASE("selectDevice only considers devices that support the required extensions", "[gpu][device]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device available");

    vk::raii::SurfaceKHR surface = createHeadlessSurface(*fixture);

    // The swapchain extension is what the Foundry always requires, so it must be available.
    std::vector<char const*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    CHECK_NOTHROW(spock::selectDevice(
        fixture->instance,
        *surface,
        vk::QueueFlagBits::eGraphics,
        vk::PhysicalDeviceType::eDiscreteGpu,
        extensions));

    extensions.push_back("VK_SPOCK_extension_that_does_not_exist");
    CHECK_THROWS_AS(
        spock::selectDevice(
            fixture->instance,
            *surface,
            vk::QueueFlagBits::eGraphics,
            vk::PhysicalDeviceType::eDiscreteGpu,
            extensions),
        std::runtime_error);
}

TEST_CASE("selectDevice is deterministic and prefers the requested device type", "[gpu][device]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device available");

    vk::raii::SurfaceKHR surface = createHeadlessSurface(*fixture);

    auto [first, firstFamilies] = spock::selectDevice(fixture->instance, *surface);
    auto [second, secondFamilies] = spock::selectDevice(fixture->instance, *surface);
    CHECK(*first == *second);
    CHECK(firstFamilies.graphics == secondFamilies.graphics);
    CHECK(firstFamilies.present == secondFamilies.present);

    // Asking for the type of the device that was chosen must keep choosing it: the preference
    // bonus outranks every fallback score.
    vk::PhysicalDeviceType chosenType = first.getProperties().deviceType;
    auto [preferred, preferredFamilies] = spock::selectDevice(
        fixture->instance,
        *surface,
        vk::QueueFlagBits::eGraphics,
        chosenType);
    CHECK(preferred.getProperties().deviceType == chosenType);
}

TEST_CASE("Foundry uses the device that selectDevice would choose", "[gpu][device]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device available");

    vk::raii::SurfaceKHR surface = createHeadlessSurface(*fixture);

    std::vector<char const*> extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
        "VK_KHR_portability_subset",
#endif
    };

    auto [physicalDevice, queueFamilies] = spock::selectDevice(
        fixture->instance,
        *surface,
        vk::QueueFlagBits::eGraphics,
        vk::PhysicalDeviceType::eDiscreteGpu,
        extensions);

    CHECK(*physicalDevice == *fixture->foundry->physicalDevice());
}

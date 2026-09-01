// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/creators.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

TEST_CASE("getDefaultInstanceExtensions returns non-empty list", "[creators]")
{
    auto extensions = spock::getDefaultInstanceExtensions();
    CHECK(!extensions.empty());
    
    // Should contain VK_KHR_SURFACE and platform-specific surface extension
    auto hasSurface = std::any_of(
        extensions.begin(), extensions.end(),
        [](const auto& ext) { return ext == "VK_KHR_surface"; });
    CHECK(hasSurface);
}

TEST_CASE("getDefaultDeviceExtensions returns non-empty list", "[creators]")
{
    auto extensions = spock::getDefaultDeviceExtensions();
    CHECK(!extensions.empty());
    
    // Should contain VK_KHR_SWAPCHAIN at minimum
    auto hasSwapchain = std::any_of(
        extensions.begin(), extensions.end(),
        [](const auto& ext) { return ext == "VK_KHR_swapchain"; });
    CHECK(hasSwapchain);
}

TEST_CASE("getDefaultInstanceExtensions does not contain duplicates", "[creators]")
{
    auto extensions = spock::getDefaultInstanceExtensions();
    auto sorted = extensions;
    std::sort(sorted.begin(), sorted.end());
    
    auto it = std::adjacent_find(sorted.begin(), sorted.end());
    CHECK(it == sorted.end());
}

TEST_CASE("getDefaultDeviceExtensions does not contain duplicates", "[creators]")
{
    auto extensions = spock::getDefaultDeviceExtensions();
    auto sorted = extensions;
    std::sort(sorted.begin(), sorted.end());
    
    auto it = std::adjacent_find(sorted.begin(), sorted.end());
    CHECK(it == sorted.end());
}

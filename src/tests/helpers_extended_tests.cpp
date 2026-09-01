// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/helpers.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("pickPresentMode prefers Mailbox when available", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifo};
    CHECK(spock::pickPresentMode(modes) == vk::PresentModeKHR::eMailbox);
}

TEST_CASE("pickPresentMode prefers Immediate over Fifo", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifo};
    CHECK(spock::pickPresentMode(modes) == vk::PresentModeKHR::eImmediate);
}

TEST_CASE("pickPresentMode returns Fifo as fallback", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo};
    CHECK(spock::pickPresentMode(modes) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("pickPresentMode handles empty list", "[helpers_extended]")
{
    CHECK(spock::pickPresentMode({}) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("pickSurfaceFormat prefers B8G8R8A8Unorm", "[helpers_extended]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
        {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}};
    
    auto picked = spock::pickSurfaceFormat(formats);
    CHECK(picked.format == vk::Format::eB8G8R8A8Unorm);
}

TEST_CASE("pickSurfaceFormat falls back to R8G8B8A8Unorm", "[helpers_extended]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}};
    
    auto picked = spock::pickSurfaceFormat(formats);
    CHECK(picked.format == vk::Format::eR8G8B8A8Unorm);
}

TEST_CASE("pickSurfaceFormat falls back to first available when no preferred format found", "[helpers_extended]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eSrgbNonlinear}};
    
    auto picked = spock::pickSurfaceFormat(formats);
    CHECK(picked.format == vk::Format::eA2B10G10R10UnormPack32);
}

TEST_CASE("clampSurfaceImageCount respects minimum bound", "[helpers_extended]")
{
    CHECK(spock::clampSurfaceImageCount(1, 3, 10) == 3);
    CHECK(spock::clampSurfaceImageCount(2, 5, 10) == 5);
}

TEST_CASE("clampSurfaceImageCount respects maximum bound", "[helpers_extended]")
{
    CHECK(spock::clampSurfaceImageCount(10, 2, 5) == 5);
    CHECK(spock::clampSurfaceImageCount(100, 2, 10) == 10);
}

TEST_CASE("clampSurfaceImageCount treats zero max as unbounded", "[helpers_extended]")
{
    CHECK(spock::clampSurfaceImageCount(100, 2, 0) == 100);
    CHECK(spock::clampSurfaceImageCount(1000, 1, 0) == 1000);
}

TEST_CASE("clampSurfaceImageCount returns value in valid range", "[helpers_extended]")
{
    CHECK(spock::clampSurfaceImageCount(5, 2, 10) == 5);
    CHECK(spock::clampSurfaceImageCount(3, 2, 8) == 3);
}

TEST_CASE("QueueIndices can store graphics and present queue family indices", "[helpers_extended]")
{
    spock::QueueIndices indices{0, 1};
    CHECK(indices.graphics == 0);
    CHECK(indices.present == 1);
}

TEST_CASE("QueueIndices can be assigned", "[helpers_extended]")
{
    spock::QueueIndices indices1{0, 0};
    spock::QueueIndices indices2{1, 2};
    
    indices1 = indices2;
    CHECK(indices1.graphics == 1);
    CHECK(indices1.present == 2);
}

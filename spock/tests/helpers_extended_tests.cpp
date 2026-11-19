// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/foundry.hpp"
#include "spock/helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>

// These cover pickPresentMode and pickCompositeAlpha with the default SwapchainOptions preferences, which are
// what every swapchain gets unless the caller passes its own.

TEST_CASE("Default present modes prefer Mailbox when available", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox};
    CHECK(spock::pickPresentMode(modes, spock::SwapchainOptions{}.presentModes) == vk::PresentModeKHR::eMailbox);
}

TEST_CASE("Default present modes prefer Immediate over Fifo", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate};
    CHECK(spock::pickPresentMode(modes, spock::SwapchainOptions{}.presentModes) == vk::PresentModeKHR::eImmediate);
}

TEST_CASE("Default present modes return Fifo as fallback", "[helpers_extended]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eFifoRelaxed};
    CHECK(spock::pickPresentMode(modes, spock::SwapchainOptions{}.presentModes) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("Default present modes handle an empty list", "[helpers_extended]")
{
    CHECK(spock::pickPresentMode({}, spock::SwapchainOptions{}.presentModes) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("Default composite alpha keeps the window opaque when the surface allows it", "[helpers_extended]")
{
    using Alpha = vk::CompositeAlphaFlagBitsKHR;
    std::vector<Alpha> const preferred = spock::SwapchainOptions{}.compositeAlpha;

    CHECK(spock::pickCompositeAlpha(Alpha::eOpaque | Alpha::ePreMultiplied | Alpha::eInherit, preferred) == Alpha::eOpaque);
    CHECK(spock::pickCompositeAlpha(Alpha::ePostMultiplied | Alpha::ePreMultiplied, preferred) == Alpha::ePreMultiplied);
    CHECK(spock::pickCompositeAlpha(Alpha::eInherit | Alpha::ePostMultiplied, preferred) == Alpha::ePostMultiplied);
    CHECK(spock::pickCompositeAlpha(Alpha::eInherit, preferred) == Alpha::eInherit);
}

TEST_CASE("pickPreTransform matches the surface's current transform", "[helpers_extended]")
{
    using Transform = vk::SurfaceTransformFlagBitsKHR;
    vk::SurfaceCapabilitiesKHR capabilities;
    capabilities.supportedTransforms = Transform::eIdentity | Transform::eRotate90 | Transform::eRotate180 | Transform::eRotate270;

    for (Transform current : {Transform::eIdentity, Transform::eRotate90, Transform::eRotate180, Transform::eRotate270})
    {
        INFO("current transform " << vk::to_string(current));
        capabilities.currentTransform = current;
        CHECK(spock::pickPreTransform(capabilities) == current);
    }
}

TEST_CASE("pickPreTransform falls back to the lowest supported transform", "[helpers_extended]")
{
    using Transform = vk::SurfaceTransformFlagBitsKHR;
    vk::SurfaceCapabilitiesKHR capabilities;
    capabilities.currentTransform = Transform::eRotate90;

    capabilities.supportedTransforms = Transform::eIdentity | Transform::eRotate180;
    CHECK(spock::pickPreTransform(capabilities) == Transform::eIdentity);

    capabilities.supportedTransforms = Transform::eRotate270 | Transform::eRotate180;
    CHECK(spock::pickPreTransform(capabilities) == Transform::eRotate180);

    capabilities.supportedTransforms = {};
    CHECK_THROWS_AS(spock::pickPreTransform(capabilities), std::runtime_error);
}

namespace
{
    vk::SurfaceCapabilitiesKHR extentCapabilities(vk::Extent2D current, vk::Extent2D minExtent, vk::Extent2D maxExtent)
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        capabilities.currentExtent = current;
        capabilities.minImageExtent = minExtent;
        capabilities.maxImageExtent = maxExtent;
        return capabilities;
    }

    constexpr uint32_t UNDEFINED_SIZE = (std::numeric_limits<uint32_t>::max)();
}

TEST_CASE("pickSwapchainExtent uses the surface's extent when it is defined", "[helpers_extended]")
{
    using Transform = vk::SurfaceTransformFlagBitsKHR;
    auto const capabilities = extentCapabilities({1080, 2400}, {1, 1}, {4096, 4096});

    CHECK(spock::pickSwapchainExtent(capabilities, {500, 500}, Transform::eIdentity) == vk::Extent2D{1080, 2400});
    CHECK(spock::pickSwapchainExtent(capabilities, {500, 500}, Transform::eRotate180) == vk::Extent2D{1080, 2400});
}

TEST_CASE("pickSwapchainExtent clamps the requested extent when the surface's is undefined", "[helpers_extended]")
{
    using Transform = vk::SurfaceTransformFlagBitsKHR;
    auto const capabilities = extentCapabilities({UNDEFINED_SIZE, UNDEFINED_SIZE}, {16, 16}, {1024, 768});

    CHECK(spock::pickSwapchainExtent(capabilities, {640, 480}, Transform::eIdentity) == vk::Extent2D{640, 480});
    CHECK(spock::pickSwapchainExtent(capabilities, {2000, 8}, Transform::eIdentity) == vk::Extent2D{1024, 16});
}

TEST_CASE("pickSwapchainExtent swaps the width and height for a 90 or 270 degree rotation", "[helpers_extended]")
{
    using Transform = vk::SurfaceTransformFlagBitsKHR;

    for (Transform rotation : {
        Transform::eRotate90,
        Transform::eRotate270,
        Transform::eHorizontalMirrorRotate90,
        Transform::eHorizontalMirrorRotate270})
    {
        INFO("pre-transform " << vk::to_string(rotation));

        // A phone in landscape gets a portrait swapchain.
        auto const defined = extentCapabilities({2400, 1080}, {1, 1}, {4096, 4096});
        CHECK(spock::pickSwapchainExtent(defined, {500, 500}, rotation) == vk::Extent2D{1080, 2400});

        // Swapped before clamping.
        auto const undefined = extentCapabilities({UNDEFINED_SIZE, UNDEFINED_SIZE}, {1, 1}, {2000, 2000});
        CHECK(spock::pickSwapchainExtent(undefined, {3000, 1000}, rotation) == vk::Extent2D{1000, 2000});
    }
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

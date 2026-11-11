// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

TEST_CASE("clampSurfaceImageCount raises the count to at least the minimum", "[helpers]")
{
    CHECK(spock::clampSurfaceImageCount(1, 3, 8) == 3);
}

TEST_CASE("clampSurfaceImageCount caps the count at the maximum", "[helpers]")
{
    CHECK(spock::clampSurfaceImageCount(10, 2, 4) == 4);
}

TEST_CASE("clampSurfaceImageCount leaves an in-range count untouched", "[helpers]")
{
    CHECK(spock::clampSurfaceImageCount(3, 2, 8) == 3);
}

TEST_CASE("clampSurfaceImageCount treats a zero maximum as unbounded", "[helpers]")
{
    // Some drivers report maxImageCount == 0 to mean "no upper limit".
    CHECK(spock::clampSurfaceImageCount(100, 2, 0) == 100);
}

TEST_CASE("findMemoryType selects the first memory type matching both the type mask and property flags", "[helpers]")
{
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    memoryProperties.memoryTypeCount = 3;
    memoryProperties.memoryTypes[0].propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    memoryProperties.memoryTypes[1].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    memoryProperties.memoryTypes[2].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;

    // typeBits selects all three candidate memory types; only index 1 has both requested property flags.
    uint32_t typeIndex = spock::findMemoryType(
        memoryProperties,
        0b111,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    CHECK(typeIndex == 1);
}

TEST_CASE("findMemoryType only considers memory types allowed by the type mask", "[helpers]")
{
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    memoryProperties.memoryTypeCount = 2;
    // Both types satisfy the property flags, but typeBits only allows index 1.
    memoryProperties.memoryTypes[0].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;
    memoryProperties.memoryTypes[1].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;

    uint32_t typeIndex = spock::findMemoryType(
        memoryProperties,
        0b10,
        vk::MemoryPropertyFlagBits::eHostVisible);

    CHECK(typeIndex == 1);
}

TEST_CASE("findMemoryType tests each type against its own bit of the type mask", "[helpers]")
{
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    memoryProperties.memoryTypeCount = 6;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        memoryProperties.memoryTypes[i].propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    }
    memoryProperties.memoryTypes[5].propertyFlags |= vk::MemoryPropertyFlagBits::eHostVisible;

    // Bits 1 and 5 are allowed; only type 5 is host visible.
    uint32_t typeIndex = spock::findMemoryType(
        memoryProperties,
        0b100010,
        vk::MemoryPropertyFlagBits::eHostVisible);

    CHECK(typeIndex == 5);
}

TEST_CASE("findMemoryType ignores mask bits beyond the memory type count", "[helpers]")
{
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    memoryProperties.memoryTypeCount = 2;
    memoryProperties.memoryTypes[0].propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    memoryProperties.memoryTypes[1].propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    // A stale entry past memoryTypeCount must never be returned.
    memoryProperties.memoryTypes[2].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;

    CHECK_THROWS_AS(
        spock::findMemoryType(memoryProperties, 0b111, vk::MemoryPropertyFlagBits::eHostVisible),
        std::runtime_error);
}

TEST_CASE("findMemoryType throws when no allowed type has the requested properties", "[helpers]")
{
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    memoryProperties.memoryTypeCount = 2;
    memoryProperties.memoryTypes[0].propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    memoryProperties.memoryTypes[1].propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;

    SECTION("no type has the properties")
    {
        CHECK_THROWS_AS(
            spock::findMemoryType(memoryProperties, 0b11, vk::MemoryPropertyFlagBits::eHostCoherent),
            std::runtime_error);
    }
    SECTION("the type with the properties is masked out")
    {
        CHECK_THROWS_AS(
            spock::findMemoryType(memoryProperties, 0b01, vk::MemoryPropertyFlagBits::eHostVisible),
            std::runtime_error);
    }
    SECTION("an empty type mask")
    {
        CHECK_THROWS_AS(
            spock::findMemoryType(memoryProperties, 0, {}),
            std::runtime_error);
    }
}

TEST_CASE("pickSurfaceFormat maps an undefined single format to a default sRGB format", "[helpers]")
{
    std::vector<vk::SurfaceFormatKHR> formats{{vk::Format::eUndefined, vk::ColorSpaceKHR::eSrgbNonlinear}};

    vk::SurfaceFormatKHR picked = spock::pickSurfaceFormat(formats);

    CHECK(picked.format == vk::Format::eB8G8R8A8Unorm);
    CHECK(picked.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
}

TEST_CASE("pickSurfaceFormat prefers B8G8R8A8Unorm when it is available", "[helpers]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eR8G8B8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
        {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
        {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}};

    vk::SurfaceFormatKHR picked = spock::pickSurfaceFormat(formats);

    CHECK(picked.format == vk::Format::eB8G8R8A8Unorm);
}

TEST_CASE("pickSurfaceFormat falls back to the next preferred format when the top choice is missing", "[helpers]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eB8G8R8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
        {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}};

    vk::SurfaceFormatKHR picked = spock::pickSurfaceFormat(formats);

    CHECK(picked.format == vk::Format::eR8G8B8A8Unorm);
}

TEST_CASE("pickSurfaceFormat falls back to the first entry when none of the preferred formats are present", "[helpers]")
{
    std::vector<vk::SurfaceFormatKHR> formats{
        {vk::Format::eA2R10G10B10UnormPack32, vk::ColorSpaceKHR::eSrgbNonlinear},
        {vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eSrgbNonlinear}};

    vk::SurfaceFormatKHR picked = spock::pickSurfaceFormat(formats);

    CHECK(picked.format == vk::Format::eA2R10G10B10UnormPack32);
}

TEST_CASE("pickPresentMode returns the first preferred mode the surface supports", "[helpers]")
{
    std::vector<vk::PresentModeKHR> supported{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox};

    CHECK(spock::pickPresentMode(supported, {vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox}) == vk::PresentModeKHR::eImmediate);
    CHECK(spock::pickPresentMode(supported, {vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate}) == vk::PresentModeKHR::eMailbox);
    CHECK(spock::pickPresentMode(supported, {vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox}) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("pickPresentMode skips preferred modes the surface does not support", "[helpers]")
{
    std::vector<vk::PresentModeKHR> supported{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate};

    CHECK(spock::pickPresentMode(supported, {vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate}) == vk::PresentModeKHR::eImmediate);
}

TEST_CASE("pickPresentMode falls back to FIFO when no preferred mode is supported", "[helpers]")
{
    std::vector<vk::PresentModeKHR> supported{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eFifoRelaxed};

    CHECK(spock::pickPresentMode(supported, {vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate}) == vk::PresentModeKHR::eFifo);
    CHECK(spock::pickPresentMode(supported, {}) == vk::PresentModeKHR::eFifo);
    CHECK(spock::pickPresentMode({}, {vk::PresentModeKHR::eMailbox}) == vk::PresentModeKHR::eFifo);
    CHECK(spock::pickPresentMode({}, {}) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("pickCompositeAlpha returns the first preferred mode the surface supports", "[helpers]")
{
    using Alpha = vk::CompositeAlphaFlagBitsKHR;
    vk::CompositeAlphaFlagsKHR supported = Alpha::eOpaque | Alpha::ePreMultiplied;

    CHECK(spock::pickCompositeAlpha(supported, {Alpha::ePreMultiplied, Alpha::eOpaque}) == Alpha::ePreMultiplied);
    CHECK(spock::pickCompositeAlpha(supported, {Alpha::eOpaque, Alpha::ePreMultiplied}) == Alpha::eOpaque);
    CHECK(spock::pickCompositeAlpha(supported, {Alpha::ePostMultiplied, Alpha::ePreMultiplied}) == Alpha::ePreMultiplied);
}

TEST_CASE("pickCompositeAlpha falls back to the first supported mode when none is preferred", "[helpers]")
{
    using Alpha = vk::CompositeAlphaFlagBitsKHR;

    // The fallback order is opaque, premultiplied, postmultiplied, inherit.
    CHECK(spock::pickCompositeAlpha(Alpha::eInherit | Alpha::ePostMultiplied, {Alpha::eOpaque}) == Alpha::ePostMultiplied);
    CHECK(spock::pickCompositeAlpha(Alpha::eInherit | Alpha::ePreMultiplied, {}) == Alpha::ePreMultiplied);
    CHECK(spock::pickCompositeAlpha(Alpha::eInherit, {Alpha::eOpaque, Alpha::ePreMultiplied}) == Alpha::eInherit);
    CHECK(spock::pickCompositeAlpha(Alpha::eOpaque | Alpha::eInherit, {}) == Alpha::eOpaque);
}

TEST_CASE("pickCompositeAlpha throws when the surface reports no mode", "[helpers]")
{
    CHECK_THROWS_AS(
        spock::pickCompositeAlpha({}, {vk::CompositeAlphaFlagBitsKHR::eOpaque}),
        std::runtime_error);
}

TEST_CASE("texelSize returns the bytes per texel of single-plane color formats", "[helpers]")
{
    CHECK(spock::texelSize(vk::Format::eR8Unorm) == 1);
    CHECK(spock::texelSize(vk::Format::eR8G8Unorm) == 2);
    CHECK(spock::texelSize(vk::Format::eR16Sfloat) == 2);
    CHECK(spock::texelSize(vk::Format::eR8G8B8A8Unorm) == 4);
    CHECK(spock::texelSize(vk::Format::eB8G8R8A8Srgb) == 4);
    CHECK(spock::texelSize(vk::Format::eA2B10G10R10UnormPack32) == 4);
    CHECK(spock::texelSize(vk::Format::eR32Sfloat) == 4);
    CHECK(spock::texelSize(vk::Format::eR16G16B16A16Sfloat) == 8);
    CHECK(spock::texelSize(vk::Format::eR32G32Sfloat) == 8);
    CHECK(spock::texelSize(vk::Format::eR32G32B32A32Sfloat) == 16);
}

TEST_CASE("texelSize throws for formats it does not know", "[helpers]")
{
    CHECK_THROWS_AS(spock::texelSize(vk::Format::eUndefined), std::invalid_argument);
    CHECK_THROWS_AS(spock::texelSize(vk::Format::eD32Sfloat), std::invalid_argument);
    CHECK_THROWS_AS(spock::texelSize(vk::Format::eBc1RgbaUnormBlock), std::invalid_argument);
    CHECK_THROWS_AS(spock::texelSize(vk::Format::eG8B8R83Plane420Unorm), std::invalid_argument);
}

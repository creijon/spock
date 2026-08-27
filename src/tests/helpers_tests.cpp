#include "spock/helpers.hpp"

#include <catch2/catch_test_macros.hpp>

// These two helpers are implemented in helpers.cpp as ordinary (non-static)
// free functions but are intentionally not exposed via helpers.hpp since
// they're implementation details of findGraphicsAndPresentQueueFamilyIndex /
// allocateDeviceMemory. They operate purely on plain data, so they're worth
// unit-testing directly via a matching forward declaration.
namespace spock
{
    uint32_t findMemoryType(
        vk::PhysicalDeviceMemoryProperties const &memoryProperties,
        uint32_t typeBits,
        vk::MemoryPropertyFlags requirementsMask);

    uint32_t findGraphicsQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties);
} // namespace spock

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

TEST_CASE("findGraphicsQueueFamilyIndex returns the first family that supports graphics", "[helpers]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[2].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK(spock::findGraphicsQueueFamilyIndex(families) == 1);
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

TEST_CASE("pickPresentMode defaults to FIFO when nothing better is offered", "[helpers]")
{
    CHECK(spock::pickPresentMode({}) == vk::PresentModeKHR::eFifo);
    CHECK(spock::pickPresentMode({vk::PresentModeKHR::eFifo}) == vk::PresentModeKHR::eFifo);
}

TEST_CASE("pickPresentMode prefers Immediate over Fifo", "[helpers]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate};
    CHECK(spock::pickPresentMode(modes) == vk::PresentModeKHR::eImmediate);
}

TEST_CASE("pickPresentMode prefers Mailbox over both Immediate and Fifo", "[helpers]")
{
    std::vector<vk::PresentModeKHR> modes{vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox};
    CHECK(spock::pickPresentMode(modes) == vk::PresentModeKHR::eMailbox);

    std::vector<vk::PresentModeKHR> reordered{vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate};
    CHECK(spock::pickPresentMode(reordered) == vk::PresentModeKHR::eMailbox);
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"

#include "spock/creators.hpp"
#include "spock/device_selector.hpp"
#include "spock/helpers.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace spock_test;

namespace
{
    // An optimally tiled, device-local color image that transfers can write and read.
    spock::ImageWrapper createTransferImage(
        spock::FoundryPtr const &foundry,
        vk::Format format,
        vk::Extent2D extent,
        vk::ImageUsageFlags extraUsage = {})
    {
        return spock::ImageWrapper(
            foundry,
            format,
            extent,
            vk::ImageTiling::eOptimal,
            extraUsage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eUndefined,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eColor);
    }

    // Copy all of a single-layer color image back to the host. `layout` is the layout the image is in, and the
    // image is put back in it afterwards.
    std::vector<uint8_t> readImage(
        spock::FoundryPtr const &foundry,
        vk::Image image,
        vk::Format format,
        vk::Extent2D extent,
        vk::ImageLayout layout)
    {
        vk::DeviceSize const size = vk::DeviceSize(extent.width) * extent.height * spock::texelSize(format);
        spock::BufferWrapper buffer(foundry, size, vk::BufferUsageFlagBits::eTransferDst);

        foundry->submit(
            [&](vk::CommandBuffer const &commandBuffer)
            {
                if (layout != vk::ImageLayout::eTransferSrcOptimal)
                {
                    spock::setImageLayout(commandBuffer, image, format, layout, vk::ImageLayout::eTransferSrcOptimal);
                }
                commandBuffer.copyImageToBuffer(
                    image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    *buffer.buffer(),
                    vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {}, vk::Extent3D(extent, 1)));
                // Make the copy visible to the host read below.
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eHost,
                    {},
                    vk::MemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eHostRead),
                    nullptr,
                    nullptr);
                if (layout != vk::ImageLayout::eTransferSrcOptimal)
                {
                    spock::setImageLayout(commandBuffer, image, format, vk::ImageLayout::eTransferSrcOptimal, layout);
                }
            });

        std::vector<uint8_t> pixels(size);
        std::memcpy(pixels.data(), buffer.map(), size);
        buffer.unmap();
        return pixels;
    }

    spock::ImageRegion region(void const *data, vk::Offset2D offset, vk::Extent2D extent, size_t rowPitch = 0)
    {
        spock::ImageRegion result;
        result.data = data;
        result.rowPitch = rowPitch;
        result.offset = offset;
        result.extent = extent;
        return result;
    }

    std::vector<uint8_t> pattern(size_t size, uint8_t seed)
    {
        std::vector<uint8_t> bytes(size);
        for (size_t i = 0; i < size; ++i)
        {
            bytes[i] = static_cast<uint8_t>(seed + i * 7);
        }
        return bytes;
    }
} // namespace

TEST_CASE("a headless Vulkan device can be created for GPU-backed tests", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    CHECK(*fixture->foundry->device() != VK_NULL_HANDLE);
}

TEST_CASE("Foundry enables timeline semaphores alongside a caller's feature chain", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    auto createFoundry = [&](void const *features) {
        spock::DeviceRequirements requirements;
        requirements.features = features;
        return spock::Foundry(
            fixture->instance,
            vk::raii::SurfaceKHR(fixture->instance, vk::HeadlessSurfaceCreateInfoEXT{}),
            requirements);
    };

    // Creating a timeline semaphore is only valid when the feature is enabled.
    auto createTimeline = [](spock::Foundry const &foundry) {
        vk::SemaphoreTypeCreateInfo type(vk::SemaphoreType::eTimeline, 0);
        return foundry.device().createSemaphore(vk::SemaphoreCreateInfo({}, &type));
    };

    SECTION("a chain without the feature")
    {
        vk::PhysicalDeviceShaderDrawParametersFeatures drawParameters(VK_FALSE);
        spock::Foundry foundry = createFoundry(&drawParameters);
        CHECK_NOTHROW(createTimeline(foundry));
    }
    SECTION("a chain that enables the feature")
    {
        vk::PhysicalDeviceVulkan12Features vulkan12;
        vulkan12.timelineSemaphore = VK_TRUE;
        spock::Foundry foundry = createFoundry(&vulkan12);
        CHECK_NOTHROW(createTimeline(foundry));
    }
    SECTION("a chain that disables the feature")
    {
        vk::PhysicalDeviceShaderDrawParametersFeatures drawParameters(VK_FALSE);
        vk::PhysicalDeviceVulkan12Features vulkan12;
        drawParameters.pNext = &vulkan12;
        CHECK_THROWS_AS(createFoundry(&drawParameters), std::invalid_argument);
    }
}

TEST_CASE("Foundry enables the optional extensions the device supports and skips the rest", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    // An extension with no dependencies that most drivers have, so the supported branch is usually exercised.
    char const *const commonExtension = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME;
    char const *const missingExtension = "VK_SPOCK_extension_that_does_not_exist";

    spock::DeviceRequirements requirements;
    // The swapchain extension is always enabled, so it doubles as an optional extension that is already requested.
    requirements.optionalExtensions = {missingExtension, commonExtension, VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    spock::Foundry foundry(
        fixture->instance,
        vk::raii::SurfaceKHR(fixture->instance, vk::HeadlessSurfaceCreateInfoEXT{}),
        requirements);

    bool const deviceHasCommon = !spock::supportedExtensions(foundry.physicalDevice(), {commonExtension}).empty();
    CHECK(foundry.hasExtension(commonExtension) == deviceHasCommon);
    CHECK_FALSE(foundry.hasExtension(missingExtension));
    CHECK(foundry.hasExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME));

    std::vector<std::string> const &extensions = foundry.extensions();
    CHECK(std::count(extensions.begin(), extensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 1);
    CHECK(std::count(extensions.begin(), extensions.end(), missingExtension) == 0);
}

TEST_CASE("Foundry::hasExtension reports the extensions the device was created with", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    for (std::string const &name : fixture->foundry->extensions())
    {
        CHECK(fixture->foundry->hasExtension(name.c_str()));
    }
    CHECK_FALSE(fixture->foundry->hasExtension("VK_SPOCK_extension_that_does_not_exist"));
    CHECK_FALSE(fixture->foundry->hasExtension(""));
}

TEST_CASE("Foundry::createSwapchain uses the present mode and composite alpha the options prefer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::Foundry const &foundry = *fixture->foundry;
    std::vector<vk::PresentModeKHR> const presentModes = supportedPresentModes(foundry);
    std::vector<vk::CompositeAlphaFlagBitsKHR> const compositeAlphas = supportedCompositeAlpha(foundry);
    REQUIRE_FALSE(presentModes.empty());
    REQUIRE_FALSE(compositeAlphas.empty());

    auto create = [&](spock::SwapchainOptions const &options) {
        return foundry.createSwapchain(vk::Extent2D{64, 64}, vk::ImageUsageFlagBits::eColorAttachment, 2, {}, options);
    };

    SECTION("the defaults")
    {
        spock::SwapchainOptions const defaults;
        spock::SwapchainInfo swapchain = foundry.createSwapchain(vk::Extent2D{64, 64}, vk::ImageUsageFlagBits::eColorAttachment, 2);
        CHECK(swapchain.presentMode == spock::pickPresentMode(presentModes, defaults.presentModes));
        CHECK(swapchain.compositeAlpha == spock::pickCompositeAlpha(
            foundry.physicalDevice().getSurfaceCapabilitiesKHR(*foundry.surface()).supportedCompositeAlpha,
            defaults.compositeAlpha));
    }
    SECTION("each supported present mode")
    {
        for (vk::PresentModeKHR mode : presentModes)
        {
            INFO("present mode " << vk::to_string(mode));
            spock::SwapchainOptions options;
            options.presentModes = {mode};
            CHECK(create(options).presentMode == mode);
        }
    }
    SECTION("FIFO when no preferred present mode is supported")
    {
        spock::SwapchainOptions options;
        options.presentModes = {};
        CHECK(create(options).presentMode == vk::PresentModeKHR::eFifo);
    }
    SECTION("each supported composite alpha")
    {
        for (vk::CompositeAlphaFlagBitsKHR mode : compositeAlphas)
        {
            INFO("composite alpha " << vk::to_string(mode));
            spock::SwapchainOptions options;
            options.compositeAlpha = {mode};
            CHECK(create(options).compositeAlpha == mode);
        }
    }
    SECTION("the first supported composite alpha when none is preferred")
    {
        spock::SwapchainOptions options;
        options.compositeAlpha = {};
        CHECK(create(options).compositeAlpha == compositeAlphas.front());
    }
}

TEST_CASE("Foundry::createSwapchain builds a swapchain for the headless surface", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::SwapchainInfo swapchain = fixture->foundry->createSwapchain(
        vk::Extent2D{256, 256},
        vk::ImageUsageFlagBits::eColorAttachment,
        2);

    CHECK(*swapchain.swapchain != VK_NULL_HANDLE);
    CHECK(swapchain.colorFormat != vk::Format::eUndefined);
    CHECK(swapchain.extent.width > 0);
    CHECK(swapchain.extent.height > 0);

    // The requested count is a minimum; the driver may hand back more images, but never fewer.
    CHECK(swapchain.swapchain.getImages().size() >= 2);
}

TEST_CASE("Foundry::createSwapchain recreates a swapchain from the old one", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::SwapchainInfo oldSwapchain = fixture->foundry->createSwapchain(
        vk::Extent2D{256, 256},
        vk::ImageUsageFlagBits::eColorAttachment,
        2);

    spock::SwapchainInfo newSwapchain = fixture->foundry->createSwapchain(
        vk::Extent2D{128, 128},
        vk::ImageUsageFlagBits::eColorAttachment,
        2,
        *oldSwapchain.swapchain);

    CHECK(*newSwapchain.swapchain != VK_NULL_HANDLE);
    CHECK(*newSwapchain.swapchain != *oldSwapchain.swapchain);
    CHECK(newSwapchain.swapchain.getImages().size() >= 2);

    // The retired swapchain must still be destroyable while its replacement is alive.
    CHECK_NOTHROW(oldSwapchain.swapchain.clear());
}

TEST_CASE("allocateDeviceMemory satisfies a real buffer's memory requirements", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::Buffer buffer(fixture->foundry->device(), vk::BufferCreateInfo({}, 256, vk::BufferUsageFlagBits::eUniformBuffer));

    vk::raii::DeviceMemory memory = spock::allocateDeviceMemory(
        fixture->foundry->device(),
        fixture->foundry->physicalDevice().getMemoryProperties(),
        buffer.getMemoryRequirements(),
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    CHECK(*memory != VK_NULL_HANDLE);
    CHECK_NOTHROW(buffer.bindMemory(memory, 0));
}

TEST_CASE("BufferWrapper uploads round-trip through mapped device memory", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    struct Uniforms
    {
        float values[4];
    };

    spock::BufferWrapper buffer(
        fixture->foundry,
        sizeof(Uniforms),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    Uniforms written{{1.0f, 2.0f, 3.0f, 4.0f}};
    buffer.upload(written);

    Uniforms readBack{};
    void *mapped = buffer.deviceMemory().mapMemory(0, sizeof(Uniforms));
    std::memcpy(&readBack, mapped, sizeof(Uniforms));
    buffer.deviceMemory().unmapMemory();

    CHECK(readBack.values[0] == 1.0f);
    CHECK(readBack.values[1] == 2.0f);
    CHECK(readBack.values[2] == 3.0f);
    CHECK(readBack.values[3] == 4.0f);
}

TEST_CASE("BufferWrapper uploads a vector of elements", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    std::vector<uint32_t> written{10, 20, 30, 40, 50};

    spock::BufferWrapper buffer(
        fixture->foundry,
        written.size() * sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer);

    buffer.upload(written);

    std::vector<uint32_t> readBack(written.size());
    void *mapped = buffer.deviceMemory().mapMemory(0, written.size() * sizeof(uint32_t));
    std::memcpy(readBack.data(), mapped, written.size() * sizeof(uint32_t));
    buffer.deviceMemory().unmapMemory();

    CHECK(readBack == written);
}

TEST_CASE("BufferWrapper uploads through an existing persistent mapping", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    std::vector<uint32_t> written{10, 20, 30, 40, 50};

    spock::BufferWrapper buffer(
        fixture->foundry,
        written.size() * sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer);

    // Vulkan doesn't allow memory to be mapped twice, so upload() has to write through this mapping.
    void *mapped = buffer.map();
    REQUIRE(mapped != nullptr);

    buffer.upload(written);

    std::vector<uint32_t> readBack(written.size());
    std::memcpy(readBack.data(), mapped, written.size() * sizeof(uint32_t));
    CHECK(readBack == written);

    buffer.upload(uint32_t{99});
    CHECK(static_cast<uint32_t const *>(mapped)[0] == 99);

    buffer.unmap();
}

TEST_CASE("BufferWrapper uploads to host visible memory that need not be coherent", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    std::vector<uint32_t> written{7, 11, 13, 17, 19, 23};
    vk::DeviceSize const size = written.size() * sizeof(uint32_t);

    // Only host visibility is asked for, so upload() has to flush in case the memory type is not coherent.
    spock::BufferWrapper source(
        fixture->foundry,
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible);
    spock::BufferWrapper destination(fixture->foundry, size, vk::BufferUsageFlagBits::eTransferDst);

    SECTION("mapped just for the upload")
    {
        source.upload(written);
    }
    SECTION("through a persistent mapping")
    {
        REQUIRE(source.map() != nullptr);
        source.upload(written);
        CHECK_NOTHROW(source.flush());
    }

    // The device reads the source, so this only sees the data if the host write was flushed.
    fixture->foundry->submit(
        [&](vk::CommandBuffer const &commandBuffer)
        {
            commandBuffer.copyBuffer(*source.buffer(), *destination.buffer(), vk::BufferCopy(0, 0, size));
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eHost,
                {},
                vk::MemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eHostRead),
                nullptr,
                nullptr);
        });

    std::vector<uint32_t> readBack(written.size());
    std::memcpy(readBack.data(), destination.map(), size);
    destination.unmap();
    source.unmap();
    CHECK(readBack == written);
}

TEST_CASE("BufferWrapper::flush does nothing for host coherent memory", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::BufferWrapper buffer(fixture->foundry, 64, vk::BufferUsageFlagBits::eStorageBuffer);

    // Coherent memory needs no flush, so it doesn't even have to be mapped.
    CHECK_NOTHROW(buffer.flush());
    CHECK_NOTHROW(buffer.flush(16, 16));
}

TEST_CASE("DepthBufferWrapper creates a bound image, memory and image view", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::DepthBufferWrapper depthBuffer(
        fixture->foundry,
        vk::Format::eD16Unorm,
        vk::Extent2D(64, 64));

    CHECK(depthBuffer.format() == vk::Format::eD16Unorm);
    CHECK(*depthBuffer.image() != VK_NULL_HANDLE);
    CHECK(*depthBuffer.imageView() != VK_NULL_HANDLE);
    CHECK(*depthBuffer.deviceMemory() != VK_NULL_HANDLE);
    CHECK(depthBuffer.extent() == vk::Extent2D(64, 64));
    CHECK(depthBuffer.layout() == vk::ImageLayout::eUndefined);

    // Move assignment carries the extent and layout along with the handles.
    depthBuffer.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    spock::DepthBufferWrapper assigned;
    assigned = std::move(depthBuffer);
    CHECK(assigned.format() == vk::Format::eD16Unorm);
    CHECK(assigned.extent() == vk::Extent2D(64, 64));
    CHECK(assigned.layout() == vk::ImageLayout::eDepthStencilAttachmentOptimal);
    CHECK(*assigned.image() != VK_NULL_HANDLE);
}

TEST_CASE("TextureWrapper constructs and accepts image data via setImage", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::TextureWrapper texture(
        fixture->foundry,
        vk::Extent2D(4, 4),
        spock::createSampler(fixture->foundry->device(), vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat));
    CHECK(*texture.sampler() != VK_NULL_HANDLE);

    CHECK_NOTHROW(fixture->foundry->submit(
        [&](vk::CommandBuffer commandBuffer)
        {
            texture.setImage(
                commandBuffer,
                [](void *data, vk::Extent2D const &extent)
                {
                    std::memset(data, 0xFF, static_cast<size_t>(extent.width) * extent.height * 4);
                });
        }));
}

TEST_CASE("TextureWrapper::setImage writes the generated pixels and leaves the image ready to sample", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    bool forceStaging = false;
    SECTION("linear tiling where the device allows it") { forceStaging = false; }
    SECTION("through a staging buffer") { forceStaging = true; }

    // A width of 5 texels is 20 bytes a row, which a linearly tiled image is likely to pad.
    vk::Extent2D const extent(5, 3);
    auto generator = [](void *data, vk::Extent2D const &size)
    {
        auto *texels = static_cast<uint8_t *>(data);
        for (uint32_t y = 0; y < size.height; ++y)
        {
            for (uint32_t x = 0; x < size.width; ++x)
            {
                for (uint32_t channel = 0; channel < 4; ++channel)
                {
                    texels[(y * size.width + x) * 4 + channel] = static_cast<uint8_t>(y * 64 + x * 8 + channel);
                }
            }
        }
    };

    // eTransferSrc so the result can be read back.
    spock::TextureWrapper texture(
        fixture->foundry,
        extent,
        spock::createSampler(fixture->foundry->device(), vk::Filter::eNearest, vk::SamplerAddressMode::eClampToEdge),
        vk::ImageUsageFlagBits::eTransferSrc,
        vk::FormatFeatureFlagBits::eTransferSrc,
        forceStaging);

    fixture->foundry->submit(
        [&](vk::CommandBuffer const &commandBuffer)
        {
            texture.setImage(commandBuffer, generator);
        });

    spock::ImageWrapper const &image = texture.image();
    CHECK(image.layout() == vk::ImageLayout::eShaderReadOnlyOptimal);
    CHECK(image.extent() == extent);

    std::vector<uint8_t> expected(size_t(extent.width) * extent.height * 4);
    generator(expected.data(), extent);
    CHECK(readImage(fixture->foundry, *image.image(), image.format(), extent, image.layout()) == expected);
}

TEST_CASE("ImageWrapper::upload copies host pixels into the image and leaves it in the final layout", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Format const format = vk::Format::eR8G8B8A8Unorm;
    vk::Extent2D const extent(4, 3);
    spock::ImageWrapper image = createTransferImage(fixture->foundry, format, extent);
    CHECK(image.extent() == extent);
    CHECK(image.layout() == vk::ImageLayout::eUndefined);

    std::vector<uint8_t> const pixels = pattern(size_t(extent.width) * extent.height * 4, 1);

    SECTION("the default final layout")
    {
        image.upload(fixture->foundry, {region(pixels.data(), {0, 0}, extent)});
        CHECK(image.layout() == vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    SECTION("a given final layout")
    {
        image.upload(fixture->foundry, {region(pixels.data(), {0, 0}, extent)}, vk::ImageLayout::eTransferSrcOptimal);
        CHECK(image.layout() == vk::ImageLayout::eTransferSrcOptimal);
    }
    SECTION("an image already in another layout")
    {
        std::vector<uint8_t> const earlier = pattern(pixels.size(), 99);
        image.upload(fixture->foundry, {region(earlier.data(), {0, 0}, extent)}, vk::ImageLayout::eGeneral);
        REQUIRE(image.layout() == vk::ImageLayout::eGeneral);
        image.upload(fixture->foundry, {region(pixels.data(), {0, 0}, extent)});
        CHECK(image.layout() == vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    CHECK(readImage(fixture->foundry, *image.image(), format, extent, image.layout()) == pixels);
}

TEST_CASE("ImageWrapper::upload only replaces the texels inside each region", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    // A one byte format means the second region's place in the staging buffer has to be aligned.
    vk::Format const format = vk::Format::eR8Unorm;
    vk::Extent2D const extent(5, 4);
    spock::ImageWrapper image = createTransferImage(fixture->foundry, format, extent);

    std::vector<uint8_t> const background(size_t(extent.width) * extent.height, 0x11);
    image.upload(fixture->foundry, {region(background.data(), {0, 0}, extent)});

    std::vector<uint8_t> const first{0xA1, 0xA2, 0xA3};       // 3x1 at (1, 0)
    std::vector<uint8_t> const second{0xB1, 0xB2, 0xB3, 0xB4}; // 2x2 at (3, 2)
    image.upload(
        fixture->foundry,
        {region(first.data(), {1, 0}, {3, 1}), region(second.data(), {3, 2}, {2, 2})});

    std::vector<uint8_t> expected = background;
    expected[0 * 5 + 1] = 0xA1;
    expected[0 * 5 + 2] = 0xA2;
    expected[0 * 5 + 3] = 0xA3;
    expected[2 * 5 + 3] = 0xB1;
    expected[2 * 5 + 4] = 0xB2;
    expected[3 * 5 + 3] = 0xB3;
    expected[3 * 5 + 4] = 0xB4;
    CHECK(readImage(fixture->foundry, *image.image(), format, extent, image.layout()) == expected);
}

TEST_CASE("ImageWrapper::upload reads source rows at the given row pitch", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Format const format = vk::Format::eR8G8B8A8Unorm;
    vk::Extent2D const extent(2, 2);
    spock::ImageWrapper image = createTransferImage(fixture->foundry, format, extent);

    // A 4x2 source picture of which only the middle 2x2 texels go into the image.
    std::array<uint32_t, 8> const source{
        0xDEAD0000, 0x00000001, 0x00000002, 0xDEAD0003,
        0xDEAD0010, 0x00000011, 0x00000012, 0xDEAD0013};
    image.upload(fixture->foundry, {region(&source[1], {0, 0}, extent, 4 * sizeof(uint32_t))});

    std::array<uint32_t, 4> const wanted{0x00000001, 0x00000002, 0x00000011, 0x00000012};
    std::vector<uint8_t> expected(sizeof(wanted));
    std::memcpy(expected.data(), wanted.data(), sizeof(wanted));
    CHECK(readImage(fixture->foundry, *image.image(), format, extent, image.layout()) == expected);
}

TEST_CASE("ImageWrapper::upload rejects regions it cannot copy", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Extent2D const extent(4, 4);
    spock::ImageWrapper image = createTransferImage(fixture->foundry, vk::Format::eR8G8B8A8Unorm, extent);
    std::vector<uint8_t> const pixels(size_t(extent.width) * extent.height * 4);
    auto const &foundry = fixture->foundry;

    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {1, 0}, {4, 4})}), std::invalid_argument);     // past the right edge
    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {0, 3}, {4, 2})}), std::invalid_argument);     // past the bottom edge
    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {-1, 0}, {2, 2})}), std::invalid_argument);    // a negative offset
    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {0, 0}, {0, 2})}), std::invalid_argument);     // no width
    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {0, 0}, {2, 0})}), std::invalid_argument);     // no height
    CHECK_THROWS_AS(image.upload(foundry, {region(nullptr, {0, 0}, {2, 2})}), std::invalid_argument);           // no data
    CHECK_THROWS_AS(image.upload(foundry, {region(pixels.data(), {0, 0}, {2, 2}, 7)}), std::invalid_argument);  // pitch shorter than a row
    // One bad region rejects the whole upload.
    CHECK_THROWS_AS(
        image.upload(foundry, {region(pixels.data(), {0, 0}, {2, 2}), region(pixels.data(), {3, 3}, {2, 2})}),
        std::invalid_argument);

    // The regions are checked before anything is recorded, so the image is untouched.
    CHECK(image.layout() == vk::ImageLayout::eUndefined);

    // An empty list is not an error, and leaves the image as it is.
    CHECK_NOTHROW(image.upload(foundry, {}));
    CHECK(image.layout() == vk::ImageLayout::eUndefined);

    // Only formats texelSize knows can be uploaded.
    spock::DepthBufferWrapper depth(foundry, vk::Format::eD16Unorm, extent);
    CHECK_THROWS_AS(depth.upload(foundry, {region(pixels.data(), {0, 0}, {2, 2})}), std::invalid_argument);
}

TEST_CASE("ImageWrapper moves carry the extent and layout", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Format const format = vk::Format::eR8G8B8A8Unorm;
    vk::Extent2D const extent(3, 2);
    spock::ImageWrapper image = createTransferImage(fixture->foundry, format, extent);
    std::vector<uint8_t> const pixels = pattern(size_t(extent.width) * extent.height * 4, 5);
    image.upload(fixture->foundry, {region(pixels.data(), {0, 0}, extent)});

    spock::ImageWrapper moved(std::move(image));
    CHECK(moved.format() == format);
    CHECK(moved.extent() == extent);
    CHECK(moved.layout() == vk::ImageLayout::eShaderReadOnlyOptimal);

    spock::ImageWrapper assigned;
    assigned = std::move(moved);
    CHECK(assigned.format() == format);
    CHECK(assigned.extent() == extent);
    CHECK(assigned.layout() == vk::ImageLayout::eShaderReadOnlyOptimal);
    CHECK(readImage(fixture->foundry, *assigned.image(), format, extent, assigned.layout()) == pixels);
}

TEST_CASE("compileShader produces a usable shader module from valid GLSL", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    static const std::string vertexSource = R"(
#version 450
layout(location = 0) in vec4 pos;
void main() { gl_Position = pos; }
)";

    vk::raii::ShaderModule module = spock::compileShader(fixture->foundry->device(), vk::ShaderStageFlagBits::eVertex, vertexSource);

    CHECK(*module != VK_NULL_HANDLE);
}

TEST_CASE("createRenderPass and createFramebuffers build a color+depth render target", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Extent2D extent(64, 64);
    vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;

    vk::raii::Image colorImage(
        fixture->foundry->device(),
        vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            colorFormat,
            vk::Extent3D(extent, 1),
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment));
    vk::raii::DeviceMemory colorMemory = spock::allocateDeviceMemory(
        fixture->foundry->device(),
        fixture->foundry->physicalDevice().getMemoryProperties(),
        colorImage.getMemoryRequirements(),
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    colorImage.bindMemory(colorMemory, 0);

    std::vector<vk::raii::ImageView> colorImageViews;
    colorImageViews.emplace_back(
        fixture->foundry->device(),
        vk::ImageViewCreateInfo({}, colorImage, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}));

    spock::DepthBufferWrapper depthBuffer(fixture->foundry, vk::Format::eD16Unorm, extent);

    vk::raii::RenderPass renderPass = spock::createRenderPass(fixture->foundry->device(), colorFormat, depthBuffer.format());
    CHECK(*renderPass != VK_NULL_HANDLE);

    std::vector<vk::raii::Framebuffer> framebuffers = spock::createFramebuffers(
        fixture->foundry->device(), renderPass, colorImageViews, &depthBuffer.imageView(), extent);

    REQUIRE(framebuffers.size() == 1);
    CHECK(*framebuffers[0] != VK_NULL_HANDLE);
}

TEST_CASE("createRenderPass needs the color image's initial layout to load its contents", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::Device const &device = fixture->foundry->device();
    vk::Format const colorFormat = vk::Format::eR8G8B8A8Unorm;
    vk::Format const depthFormat = vk::Format::eD16Unorm;

    // The contents of an image in eUndefined may be discarded, so there would be nothing to load.
    CHECK_THROWS_AS(
        spock::createRenderPass(device, colorFormat, vk::Format::eUndefined, vk::AttachmentLoadOp::eLoad),
        std::invalid_argument);
    CHECK_THROWS_AS(
        spock::createRenderPass(device, colorFormat, depthFormat, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eUndefined),
        std::invalid_argument);

    // Clearing or discarding never reads the old contents, so those may start from eUndefined.
    CHECK_NOTHROW(spock::createRenderPass(device, colorFormat, depthFormat, vk::AttachmentLoadOp::eClear));
    CHECK_NOTHROW(spock::createRenderPass(device, colorFormat, depthFormat, vk::AttachmentLoadOp::eDontCare));

    vk::raii::RenderPass loading = spock::createRenderPass(
        device, colorFormat, depthFormat, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eColorAttachmentOptimal);
    CHECK(*loading != VK_NULL_HANDLE);
}

TEST_CASE("createRenderPass results can be read by later commands, and loaded by a later pass", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::FoundryPtr const &foundry = fixture->foundry;
    vk::raii::Device const &device = foundry->device();
    vk::Format const format = vk::Format::eR8G8B8A8Unorm;
    vk::Extent2D const extent(4, 4);

    spock::ImageWrapper color = createTransferImage(foundry, format, extent, vk::ImageUsageFlagBits::eColorAttachment);
    std::vector<vk::raii::ImageView> views;
    views.emplace_back(
        device,
        vk::ImageViewCreateInfo({}, *color.image(), vk::ImageViewType::e2D, format, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}));

    // The first pass clears the image and leaves it for a copy. The second loads it and draws nothing.
    vk::raii::RenderPass clearPass = spock::createRenderPass(
        device, format, vk::Format::eUndefined, vk::AttachmentLoadOp::eClear, vk::ImageLayout::eTransferSrcOptimal);
    vk::raii::RenderPass loadPass = spock::createRenderPass(
        device, format, vk::Format::eUndefined, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal);
    std::vector<vk::raii::Framebuffer> clearFramebuffers = spock::createFramebuffers(device, clearPass, views, nullptr, extent);
    std::vector<vk::raii::Framebuffer> loadFramebuffers = spock::createFramebuffers(device, loadPass, views, nullptr, extent);

    auto runPass = [&](vk::raii::RenderPass const &pass, vk::raii::Framebuffer const &framebuffer, std::array<float, 4> clearColor)
    {
        vk::ClearValue const clearValue(vk::ClearColorValue{clearColor});
        foundry->submit(
            [&](vk::CommandBuffer const &commandBuffer)
            {
                commandBuffer.beginRenderPass(
                    vk::RenderPassBeginInfo(*pass, *framebuffer, vk::Rect2D({0, 0}, extent), clearValue),
                    vk::SubpassContents::eInline);
                commandBuffer.endRenderPass();
            });
    };

    std::vector<uint8_t> green;
    for (uint32_t texel = 0; texel < extent.width * extent.height; ++texel)
    {
        green.insert(green.end(), {0x00, 0xFF, 0x00, 0xFF});
    }

    runPass(clearPass, clearFramebuffers.front(), {0.0f, 1.0f, 0.0f, 1.0f});
    color.setLayout(vk::ImageLayout::eTransferSrcOptimal);
    CHECK(readImage(foundry, *color.image(), format, extent, color.layout()) == green);

    // A load ignores the clear value, so the red here must not show up.
    runPass(loadPass, loadFramebuffers.front(), {1.0f, 0.0f, 0.0f, 1.0f});
    CHECK(readImage(foundry, *color.image(), format, extent, color.layout()) == green);
}

TEST_CASE("createCommandBuffer allocates a primary command buffer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::CommandBuffer commandBuffer =
        spock::createCommandBuffer(fixture->foundry->device(), fixture->foundry->commandPool());

    CHECK(*commandBuffer != VK_NULL_HANDLE);
    CHECK_NOTHROW(commandBuffer.begin(vk::CommandBufferBeginInfo()));
    CHECK_NOTHROW(commandBuffer.end());
}

TEST_CASE("Foundry leaves the optional queues null unless they are required", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    // The fixture asks for the default requirements, which are graphics only.
    CHECK(*fixture->foundry->computeCommandPool() == VK_NULL_HANDLE);
    CHECK(*fixture->foundry->computeQueue() == VK_NULL_HANDLE);
    CHECK(*fixture->foundry->transferCommandPool() == VK_NULL_HANDLE);
    CHECK(*fixture->foundry->transferQueue() == VK_NULL_HANDLE);
    CHECK(*fixture->foundry->videoDecodeQueue() == VK_NULL_HANDLE);
}

TEST_CASE("Foundry owns a compute command pool and queue when compute is required, and can submit one-time commands to them", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::DeviceRequirements requirements;
    requirements.queues = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    spock::Foundry const foundry(
        fixture->instance,
        vk::raii::SurfaceKHR(fixture->instance, vk::HeadlessSurfaceCreateInfoEXT{}),
        requirements);

    CHECK(*foundry.computeCommandPool() != VK_NULL_HANDLE);
    CHECK(*foundry.computeQueue() != VK_NULL_HANDLE);

    bool recorded = false;
    CHECK_NOTHROW(foundry.submitCompute(
        [&](vk::CommandBuffer const &commandBuffer)
        {
            recorded = true;
            // A no-op barrier is enough to prove a real command buffer was
            // recorded and submitted through the compute queue.
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::DependencyFlags(),
                nullptr,
                nullptr,
                nullptr);
        }));
    CHECK(recorded);
}

TEST_CASE("setImageLayout records a barrier for every layout it knows and rejects the others", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::FoundryPtr const &foundry = fixture->foundry;
    vk::Format const colorFormat = vk::Format::eR8G8B8A8Unorm;
    vk::Format const depthFormat = vk::Format::eD16Unorm;
    spock::ImageWrapper color = createTransferImage(foundry, colorFormat, {4, 4}, vk::ImageUsageFlagBits::eColorAttachment);
    spock::ImageWrapper depth(
        foundry,
        depthFormat,
        {4, 4},
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eUndefined,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageAspectFlagBits::eDepth);

    // The barriers are only recorded, never submitted, so the layouts don't have to be the images' real ones.
    vk::raii::CommandBuffer commandBuffer = spock::createCommandBuffer(foundry->device(), foundry->commandPool());
    commandBuffer.begin(vk::CommandBufferBeginInfo());

    SECTION("color layouts")
    {
        std::array const oldLayouts{
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePreinitialized,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR};
        std::array const newLayouts{
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR};
        for (vk::ImageLayout oldLayout : oldLayouts)
        {
            for (vk::ImageLayout newLayout : newLayouts)
            {
                INFO(vk::to_string(oldLayout) << " -> " << vk::to_string(newLayout));
                CHECK_NOTHROW(spock::setImageLayout(*commandBuffer, *color.image(), colorFormat, oldLayout, newLayout));
            }
        }
    }
    SECTION("depth layouts")
    {
        // A depth image keeps its depth aspect when it leaves the attachment layout to be sampled.
        CHECK_NOTHROW(spock::setImageLayout(*commandBuffer, *depth.image(), depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal));
        CHECK_NOTHROW(spock::setImageLayout(*commandBuffer, *depth.image(), depthFormat, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal));
        CHECK_NOTHROW(spock::setImageLayout(*commandBuffer, *depth.image(), depthFormat, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal));
    }
    SECTION("unknown layouts")
    {
        vk::Image const image = *color.image();
        CHECK_THROWS_AS(
            spock::setImageLayout(*commandBuffer, image, colorFormat, vk::ImageLayout::eDepthReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal),
            std::invalid_argument);
        CHECK_THROWS_AS(
            spock::setImageLayout(*commandBuffer, image, colorFormat, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eDepthReadOnlyOptimal),
            std::invalid_argument);
        // Nothing can be transitioned into eUndefined or ePreinitialized.
        CHECK_THROWS_AS(
            spock::setImageLayout(*commandBuffer, image, colorFormat, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eUndefined),
            std::invalid_argument);
        CHECK_THROWS_AS(
            spock::setImageLayout(*commandBuffer, image, colorFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::ePreinitialized),
            std::invalid_argument);
    }

    commandBuffer.end();
}

TEST_CASE("createDescriptorSetLayout, createDescriptorPool and updateDescriptorSets wire up a uniform buffer binding", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::DescriptorSetLayout descriptorSetLayout = spock::createDescriptorSetLayout(
        fixture->foundry->device(),
        vk::ShaderStageFlagBits::eVertex,
        {vk::DescriptorType::eUniformBuffer});
    CHECK(*descriptorSetLayout != VK_NULL_HANDLE);

    vk::raii::DescriptorPool descriptorPool = spock::createDescriptorPool(
        fixture->foundry->device(),
        {{vk::DescriptorType::eUniformBuffer, 1}});
    CHECK(*descriptorPool != VK_NULL_HANDLE);

    vk::raii::DescriptorSets descriptorSets(
        fixture->foundry->device(), vk::DescriptorSetAllocateInfo(descriptorPool, *descriptorSetLayout));
    vk::raii::DescriptorSet descriptorSet = std::move(descriptorSets.front());

    spock::BufferWrapper uniformBuffer(
        fixture->foundry, sizeof(float) * 16, vk::BufferUsageFlagBits::eUniformBuffer);

    CHECK_NOTHROW(spock::updateDescriptorSets(fixture->foundry->device(), descriptorSet, {uniformBuffer}, {}));
}

TEST_CASE("createGraphicsPipeline builds a pipeline from compiled shaders and a push-constant layout", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    static const std::string vertexSource = R"(
#version 450
layout(push_constant) uniform PushConstants { mat4 mvp; } pc;
layout(location = 0) in vec4 pos;
void main() { gl_Position = pc.mvp * pos; }
)";
    static const std::string fragmentSource = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0); }
)";

    vk::raii::ShaderModule vertexModule = spock::compileShader(fixture->foundry->device(), vk::ShaderStageFlagBits::eVertex, vertexSource);
    vk::raii::ShaderModule fragmentModule = spock::compileShader(fixture->foundry->device(), vk::ShaderStageFlagBits::eFragment, fragmentSource);

    vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;
    vk::raii::RenderPass renderPass = spock::createRenderPass(fixture->foundry->device(), colorFormat, vk::Format::eUndefined);

    vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 16);
    vk::raii::PipelineLayout pipelineLayout(fixture->foundry->device(), vk::PipelineLayoutCreateInfo({}, {}, pushConstantRange));

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo = {
            {{}, vk::ShaderStageFlagBits::eVertex, *vertexModule, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragmentModule, "main"} };

    spock::VertexFormat vertexFormat{{ { vk::Format::eR32G32B32A32Sfloat, 0 } }, sizeof(float) * 4};

    // The color blend state is optional, so pass it through only when given.
    auto createPipeline = [&](auto const &...colorBlend) {
        return spock::createGraphicsPipeline(
            fixture->foundry->device(),
            shaderStagesInfo,
            pipelineLayout,
            renderPass,
            vertexFormat,
            vk::PrimitiveTopology::eTriangleList,
            vk::CullModeFlagBits::eNone,
            false,
            colorBlend...);
    };

    SECTION("the default blending")
    {
        CHECK(*createPipeline() != VK_NULL_HANDLE);
    }
    SECTION("straight alpha")
    {
        CHECK(*createPipeline(spock::blendStraightAlpha()) != VK_NULL_HANDLE);
    }
    SECTION("premultiplied alpha")
    {
        CHECK(*createPipeline(spock::blendPremultipliedAlpha()) != VK_NULL_HANDLE);
    }
    SECTION("no blending")
    {
        CHECK(*createPipeline(spock::blendDisabled()) != VK_NULL_HANDLE);
    }
}

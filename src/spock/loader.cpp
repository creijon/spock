// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "loader.hpp"

#include "helpers.hpp"
#include "foundry.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <memory>
#include <utility>

namespace spock
{

namespace
{

using ImagePixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;

// Decodes an image file to tightly packed 8-bit RGBA, whatever its channel count on disk.
ImagePixels loadRgba(std::string const &path, unsigned &width, unsigned &height)
{
    int w = 0;
    int h = 0;
    int channels = 0;
    ImagePixels pixels(
        stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha),
        &stbi_image_free);
    if (!pixels)
    {
        throw std::runtime_error("Failed to load texture '" + path + "': " + stbi_failure_reason());
    }
    width = static_cast<unsigned>(w);
    height = static_cast<unsigned>(h);
    return pixels;
}

} // namespace

TextureWrapper Loader::texture(
    std::shared_ptr<const Foundry> const &foundry,
    std::string const &path,
    vk::raii::Sampler sampler)
{
    unsigned width = 0;
    unsigned height = 0;
    auto pixels = loadRgba(path, width, height);

    TextureWrapper texture(foundry, vk::Extent2D(width, height), std::move(sampler));

    foundry->submit(
        [&](vk::CommandBuffer commandBuffer)
        {
            texture.setImage(
                commandBuffer,
                [&pixels](void *data, vk::Extent2D const &extent)
                {
                    std::memcpy(data, pixels.get(), static_cast<size_t>(extent.width) * extent.height * 4);
                });
        });

    return texture;
}

CubemapWrapper Loader::cubemap(
    std::shared_ptr<const Foundry> const &foundry,
    std::array<std::string, CUBEMAP_FACE_COUNT> const &paths,
    vk::raii::Sampler sampler)
{
    std::vector<ImagePixels> facePixels;
    facePixels.reserve(CUBEMAP_FACE_COUNT);
    unsigned size = 0;

    for (uint32_t i = 0; i < CUBEMAP_FACE_COUNT; ++i)
    {
        unsigned width = 0;
        unsigned height = 0;
        facePixels.push_back(loadRgba(paths[i], width, height));
        if (width != height || (i > 0 && width != size))
        {
            throw std::runtime_error("Cubemap face '" + paths[i] + "' must be square and the same size as the other faces");
        }
        size = width;
    }

    vk::DeviceSize faceBytes = static_cast<vk::DeviceSize>(size) * size * 4;
    BufferWrapper stagingBuffer(
        foundry,
        faceBytes * CUBEMAP_FACE_COUNT,
        vk::BufferUsageFlagBits::eTransferSrc);
    uint8_t *staging = static_cast<uint8_t *>(stagingBuffer.deviceMemory().mapMemory(0, faceBytes * CUBEMAP_FACE_COUNT));
    for (uint32_t i = 0; i < CUBEMAP_FACE_COUNT; ++i)
    {
        std::memcpy(staging + i * faceBytes, facePixels[i].get(), faceBytes);
    }
    stagingBuffer.deviceMemory().unmapMemory();

    CubemapWrapper cubemap;
    cubemap.image = ImageWrapper(
        foundry,
        vk::Format::eR8G8B8A8Unorm,
        vk::Extent2D(size, size),
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eUndefined,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageAspectFlagBits::eColor,
        CUBEMAP_FACE_COUNT,
        vk::ImageCreateFlagBits::eCubeCompatible,
        vk::ImageViewType::eCube);

    foundry->submit(
        [&](vk::CommandBuffer commandBuffer)
        {
            setImageLayout(
                commandBuffer,
                cubemap.image.image(),
                cubemap.image.format(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                CUBEMAP_FACE_COUNT);

            // The faces are tightly packed in the staging buffer, so one copy covers all six layers.
            vk::BufferImageCopy copyRegion(
                0,
                size,
                size,
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, CUBEMAP_FACE_COUNT),
                vk::Offset3D(0, 0, 0),
                vk::Extent3D(size, size, 1));
            commandBuffer.copyBufferToImage(
                *stagingBuffer.buffer(),
                *cubemap.image.image(),
                vk::ImageLayout::eTransferDstOptimal,
                copyRegion);

            setImageLayout(
                commandBuffer,
                cubemap.image.image(),
                cubemap.image.format(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                CUBEMAP_FACE_COUNT);
        });

    cubemap.sampler = std::move(sampler);

    return cubemap;
}

}

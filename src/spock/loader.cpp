// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "loader.hpp"

#include "helpers.hpp"
#include "foundry.hpp"

#include "lodepng.h"

namespace spock
{

TextureWrapper Loader::texture(
    std::shared_ptr<const Foundry> const &foundry,
    vk::raii::Queue queue,
    std::string const &path)
{
    std::vector<unsigned char> pixels;
    unsigned width = 0;
    unsigned height = 0;
    unsigned error = lodepng::decode(pixels, width, height, path);
    if (error)
    {
        throw std::runtime_error("Failed to load texture '" + path + "': " + lodepng_error_text(error));
    }

    TextureWrapper texture(foundry, vk::Extent2D(width, height));

    oneTimeSubmit(
        foundry->device(),
        foundry->commandPool(),
        queue,
        [&](vk::CommandBuffer commandBuffer)
        {
            texture.setImage(
                commandBuffer,
                [&pixels](void *data, vk::Extent2D const &extent)
                {
                    std::memcpy(data, pixels.data(), static_cast<size_t>(extent.width) * extent.height * 4);
                });
        });

    return texture;
}

CubemapWrapper Loader::cubemap(
    std::shared_ptr<const Foundry> const &foundry,
    vk::raii::Queue const &queue,
    std::array<std::string, CUBEMAP_FACE_COUNT> const &paths)
{
    std::array<std::vector<unsigned char>, CUBEMAP_FACE_COUNT> facePixels;
    unsigned size = 0;

    for (uint32_t i = 0; i < CUBEMAP_FACE_COUNT; ++i)
    {
        unsigned width = 0;
        unsigned height = 0;
        unsigned error = lodepng::decode(facePixels[i], width, height, paths[i]);
        if (error)
        {
            throw std::runtime_error("Failed to load texture '" + paths[i] + "': " + lodepng_error_text(error));
        }
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
        std::memcpy(staging + i * faceBytes, facePixels[i].data(), faceBytes);
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

    oneTimeSubmit(
        foundry->device(),
        foundry->commandPool(),
        queue,
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

    cubemap.sampler = vk::raii::Sampler(
        foundry->device(),
        {{},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f,
        false,
        16.0f,
        false,
        vk::CompareOp::eNever,
        0.0f,
        0.0f,
        vk::BorderColor::eFloatOpaqueBlack});

    return cubemap;
}

}

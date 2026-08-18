// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace spock
{
    struct BufferWrapper
    {
        BufferWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::DeviceSize size,
            vk::BufferUsageFlags usage,
            vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        BufferWrapper() = default;
        BufferWrapper(const BufferWrapper &) = delete;
        BufferWrapper(BufferWrapper &&other) noexcept;
        BufferWrapper const& operator=(BufferWrapper&& other);

        template <typename DataType>
        void upload(
            DataType const &data) const
        {
            assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
            assert(sizeof(DataType) <= m_size);

            void *dataPtr = deviceMemory.mapMemory(0, sizeof(DataType));
            memcpy(dataPtr, &data, sizeof(DataType));
            deviceMemory.unmapMemory();
        }

        template <typename DataType>
        void upload(
            std::vector<DataType> const &data,
            size_t stride = 0) const
        {
            assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

            size_t elementSize = stride ? stride : sizeof(DataType);
            assert(sizeof(DataType) <= elementSize);

            copyToDevice(deviceMemory, data.data(), data.size(), elementSize);
        }

        template <typename DataType>
        void upload(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::raii::CommandPool const &commandPool,
            vk::raii::Queue const &queue,
            std::vector<DataType> const &data,
            size_t stride) const
        {
            assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
            assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

            size_t elementSize = stride ? stride : sizeof(DataType);
            assert(sizeof(DataType) <= elementSize);

            size_t dataSize = data.size() * elementSize;
            assert(dataSize <= m_size);

            BufferWrapper stagingBuffer(physicalDevice, device, dataSize, vk::BufferUsageFlagBits::eTransferSrc);
            copyToDevice(stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

            oneTimeSubmit(device,
                          commandPool,
                          queue,
                          [&](vk::raii::CommandBuffer const &commandBuffer)
                          { commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize)); });
        }

        // Declare the memory FIRST
        // It must live longer than the buffer that binds to it.
        vk::raii::DeviceMemory deviceMemory{nullptr};

        // Declare the buffer SECOND so that it is destroyed first.
        vk::raii::Buffer buffer{nullptr};
#if !defined(NDEBUG)
    private:
        vk::DeviceSize m_size{0};
        vk::BufferUsageFlags m_usage{};
        vk::MemoryPropertyFlags m_propertyFlags{};
#endif
    };

    struct ImageWrapper
    {
        ImageWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::Format format_,
            vk::Extent2D const &extent,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usage,
            vk::ImageLayout initialLayout,
            vk::MemoryPropertyFlags memoryProperties,
            vk::ImageAspectFlags aspectMask);
        ImageWrapper() = default;
        ImageWrapper(const ImageWrapper &) = delete;
        ImageWrapper(ImageWrapper&& other) noexcept;
        ImageWrapper const& operator=(ImageWrapper&& other);

        // the DeviceMemory should be destroyed before the Image it is bound to; to get that order with the standard destructor
        // of the ImageWrapper, the order of DeviceMemory and Image here matters
        vk::Format format;
        vk::raii::DeviceMemory deviceMemory{nullptr};
        vk::raii::Image image{nullptr};
        vk::raii::ImageView imageView{nullptr};
    };

    struct DepthBufferWrapper : public ImageWrapper
    {
        DepthBufferWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::Format format,
            vk::Extent2D const &extent);
        DepthBufferWrapper() = default;
        DepthBufferWrapper(const DepthBufferWrapper &) = delete;
        DepthBufferWrapper(DepthBufferWrapper&& other) noexcept;
        DepthBufferWrapper const& operator=(DepthBufferWrapper&& other);
    };

    struct TextureWrapper
    {
        TextureWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::Extent2D const &extent_ = {256, 256},
            vk::ImageUsageFlags usageFlags = {},
            vk::FormatFeatureFlags formatFeatureFlags = {},
            bool anisotropyEnable = false,
            bool forceStaging = false);
        TextureWrapper() = default;
        TextureWrapper(const TextureWrapper &) = delete;
        TextureWrapper(TextureWrapper &&other) noexcept;
        TextureWrapper const& operator=(TextureWrapper&& other);

        template <typename ImageGenerator>
        void setImage(
            vk::raii::CommandBuffer const &commandBuffer,
            ImageGenerator const &imageGenerator)
        {
            void *data = needsStaging ? stagingBuffer.deviceMemory.mapMemory(0, stagingBuffer.buffer.getMemoryRequirements().size)
                                      : image.deviceMemory.mapMemory(0, image.image.getMemoryRequirements().size);
            imageGenerator(data, extent);
            needsStaging ? stagingBuffer.deviceMemory.unmapMemory() : image.deviceMemory.unmapMemory();

            if (needsStaging)
            {
                // Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
                setImageLayout(
                    commandBuffer, image.image, image.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                vk::BufferImageCopy copyRegion(0,
                                               extent.width,
                                               extent.height,
                                               vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                                               vk::Offset3D(0, 0, 0),
                                               vk::Extent3D(extent, 1));
                commandBuffer.copyBufferToImage(stagingBuffer.buffer, image.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
                // Set the layout for the texture image from eTransferDstOptimal to eShaderReadOnlyOptimal
                setImageLayout(
                    commandBuffer, image.image, image.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            else
            {
                // If we can use the linear tiled image as a texture, just do it
                setImageLayout(
                    commandBuffer, image.image, image.format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }

        vk::Format format;
        vk::Extent2D extent;
        bool needsStaging;
        BufferWrapper stagingBuffer;
        ImageWrapper image;
        vk::raii::Sampler sampler{nullptr};
    };
} // namespace spock

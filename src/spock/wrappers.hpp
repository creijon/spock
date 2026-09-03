// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace spock
{
    class VertexFormat
    {
    public:
        using Attributes = std::vector<std::pair<vk::Format, size_t>>;

        VertexFormat() = default;
        VertexFormat(Attributes const& attributes, uint32_t stride);

        // VertexType must provide a static method attributes() returning
        // (vk::Format, offset) pairs, where each offset is relative to VertexType.
        template <typename VertexType>
        void addAttributes(
            uint32_t binding = 0,
            vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex)
        {
            addAttributes(VertexType::attributes(), sizeof(VertexType), binding, inputRate);
        }

        void addAttributes(
            Attributes const& attributes,
            uint32_t stride,
            uint32_t binding = 0,
            vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex);

        explicit operator vk::PipelineVertexInputStateCreateInfo() const;

    private:
        std::vector<vk::VertexInputBindingDescription> m_bindings;
        std::vector<vk::VertexInputAttributeDescription> m_attributes;
    };

    struct BindingData
    {
        uint32_t binding;
        vk::DescriptorType type;
        uint32_t count;
        vk::ShaderStageFlags stageFlags;
    };
    
    struct BufferUpdateData
    {
        vk::DescriptorType type;
        vk::raii::Buffer const& buffer;
        vk::DeviceSize size;
        vk::raii::BufferView const* bufferView;
    };

    // VertexType must provide static method attributes() returning
    // (vk::Format, offset) pairs, where each offset is relative to VertexType.
    template <typename VertexType>
    class VertexFormatWrapper : public VertexFormat
    {
    public:
        VertexFormatWrapper()
        {
            addAttributes<VertexType>();
        }
    };

    class BufferWrapper
    {
    public:
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

        vk::raii::DeviceMemory const& deviceMemory() const
        {
            return m_deviceMemory;
        }

        vk::raii::Buffer const& buffer() const
        {
            return m_buffer;
        }

        template <typename DataType>
        void upload(
            DataType const &data) const
        {
            assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) &&
                   (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
            assert(sizeof(DataType) <= m_size);

            void *dataPtr = m_deviceMemory.mapMemory(0, sizeof(DataType));
            memcpy(dataPtr, &data, sizeof(DataType));
            m_deviceMemory.unmapMemory();
        }

        template <typename DataType>
        void upload(
            std::vector<DataType> const &data,
            size_t stride = 0) const
        {
            assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

            size_t elementSize = stride ? stride : sizeof(DataType);
            assert(sizeof(DataType) <= elementSize);

            copyToDevice(m_deviceMemory, data.data(), data.size(), elementSize);
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

            BufferWrapper stagingBuffer(
                physicalDevice,
                device,
                dataSize,
                vk::BufferUsageFlagBits::eTransferSrc);
            copyToDevice(stagingBuffer.m_deviceMemory, data.data(), data.size(), elementSize);

            oneTimeSubmit(device,
                          commandPool,
                          queue,
                          [&](vk::raii::CommandBuffer const &commandBuffer)
                          { commandBuffer.copyBuffer(*stagingBuffer.m_buffer, *m_buffer, vk::BufferCopy(0, 0, dataSize)); });
        }

    protected:
        // Declare the memory FIRST
        // It must live longer than the buffer that binds to it.
        vk::raii::DeviceMemory m_deviceMemory{nullptr};

        // Declare the buffer SECOND so that it is destroyed first.
        vk::raii::Buffer m_buffer{nullptr};
#if !defined(NDEBUG)
    private:
        vk::DeviceSize m_size{0};
        vk::BufferUsageFlags m_usage{};
        vk::MemoryPropertyFlags m_propertyFlags{};
#endif
    };

    class ImageWrapper
    {
    public:
        ImageWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::Format format,
            vk::Extent2D extent,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usage,
            vk::ImageLayout initialLayout,
            vk::MemoryPropertyFlags memoryProperties,
            vk::ImageAspectFlags aspectMask);
        ImageWrapper() = default;
        ImageWrapper(const ImageWrapper &) = delete;
        ImageWrapper(ImageWrapper&& other) noexcept;
        ImageWrapper const& operator=(ImageWrapper&& other);

        vk::Format format() const
        {
            return m_format;
        }

        vk::raii::DeviceMemory const& deviceMemory() const
        {
            return m_deviceMemory;
        }
    
        vk::raii::Image const& image() const
        {
            return m_image;
        }
    
        vk::raii::ImageView const& imageView() const
        {
            return m_imageView;
        }

    protected:
        // the DeviceMemory should be destroyed before the Image it is bound to;
        // to get that order with the standard destructor of the ImageWrapper,
        // the order of DeviceMemory and Image here matters.
        vk::Format m_format{vk::Format::eUndefined};
        vk::raii::DeviceMemory m_deviceMemory{nullptr};
        vk::raii::Image m_image{nullptr};
        vk::raii::ImageView m_imageView{nullptr};
    };

    class DepthBufferWrapper : public ImageWrapper
    {
    public:
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

    class TextureWrapper
    {
    public:
        TextureWrapper(
            vk::raii::PhysicalDevice const &physicalDevice,
            vk::raii::Device const &device,
            vk::Extent2D extent,
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
            if (m_needsStaging)
            {
                void *data = m_stagingBuffer.deviceMemory().mapMemory(0, m_stagingBuffer.buffer().getMemoryRequirements().size);
                imageGenerator(data, m_extent);
                m_stagingBuffer.deviceMemory().unmapMemory();

                // Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
                setImageLayout(
                    commandBuffer,
                    m_image.image(),
                    m_image.format(),
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal);
                vk::BufferImageCopy copyRegion(
                    0,
                    m_extent.width,
                    m_extent.height,
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                    vk::Offset3D(0, 0, 0),
                    vk::Extent3D(m_extent, 1));
                commandBuffer.copyBufferToImage(
                    m_stagingBuffer.buffer(),
                    m_image.image(),
                    vk::ImageLayout::eTransferDstOptimal,
                    copyRegion);

                // Set the layout for the texture image from eTransferDstOptimal to eShaderReadOnlyOptimal
                setImageLayout(
                    commandBuffer,
                    m_image.image(),
                    m_image.format(),
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            else
            {
                void *data = m_image.deviceMemory().mapMemory(0, m_image.image().getMemoryRequirements().size);
                imageGenerator(data, m_extent);
                m_image.deviceMemory().unmapMemory();

                // If we can use the linear tiled image as a texture directly.
                setImageLayout(
                    commandBuffer,
                    m_image.image(),
                    m_image.format(),
                    vk::ImageLayout::ePreinitialized,
                    vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }

        ImageWrapper const& image() const
        {
            return m_image;
        }
    
        vk::raii::Sampler const& sampler() const
        {
            return m_sampler;
        }

    private:
        vk::Format m_format;
        vk::Extent2D m_extent;
        bool m_needsStaging;
        BufferWrapper m_stagingBuffer;
        ImageWrapper m_image;
        vk::raii::Sampler m_sampler{nullptr};
    };

} // namespace spock

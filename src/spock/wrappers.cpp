// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "wrappers.hpp"

#include "helpers.hpp"

#include <iostream>

namespace spock
{

    VertexFormat::VertexFormat(
        Attributes const& attributes,
        uint32_t stride)
    {
        m_bindings.emplace_back(0, stride);

        uint32_t location = 0;
        for (const auto& attr : attributes)
        {
            m_attributes.emplace_back(location, 0, attr.first, uint32_t(attr.second));
            ++location;
        }
    }

    void VertexFormat::addAttributes(
        Attributes const& attributes,
        uint32_t stride,
        uint32_t binding,
        vk::VertexInputRate inputRate)
    {
        // Check to see whether there is already a binding on this slot.
        auto it = std::find_if(
            m_bindings.begin(), m_bindings.end(),
            [binding](auto desc) { return desc.binding == binding; });

        if (it == m_bindings.end())
        {
            // If the isn't a binding, create one.
            m_bindings.emplace_back(binding, stride, inputRate);
        }

        // Attributes are sorted, so the last one will have the highest location.
        uint32_t location = (!m_attributes.empty()) ? m_attributes.back().location + 1 : 0;

        // Populate the attributes.
        for (const auto& attr : attributes)
        {
            m_attributes.emplace_back(location, binding, attr.first, uint32_t(attr.second));
            ++location;
        }
    }

    VertexFormat::operator vk::PipelineVertexInputStateCreateInfo() const
    {
        return { vk::PipelineVertexInputStateCreateFlags(), m_bindings, m_attributes };
    }

    BufferWrapper::BufferWrapper(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags)
        : m_buffer(device, vk::BufferCreateInfo({}, size, usage))
#if !defined(NDEBUG)
        , m_size(size)
        , m_usage(usage)
        , m_propertyFlags(propertyFlags)
#endif
    {
        m_deviceMemory = allocateDeviceMemory(
            device,
            physicalDevice.getMemoryProperties(),
            m_buffer.getMemoryRequirements(),
            propertyFlags);
        m_buffer.bindMemory(m_deviceMemory, 0);
    }

    BufferWrapper::BufferWrapper(BufferWrapper &&other) noexcept
        : m_deviceMemory(std::move(other.m_deviceMemory))
        , m_buffer(std::move(other.m_buffer))
#if !defined(NDEBUG)
        , m_size(other.m_size)
        , m_usage(other.m_usage)
        , m_propertyFlags(other.m_propertyFlags)
#endif
    {
    }

    BufferWrapper const& BufferWrapper::operator=(BufferWrapper&& other)
    {
        if (this != &other)
        {
            m_deviceMemory = std::move(other.m_deviceMemory);
            m_buffer = std::move(other.m_buffer);
#if !defined(NDEBUG)
            m_size = other.m_size;
            m_usage = other.m_usage;
            m_propertyFlags = other.m_propertyFlags;
#endif
        }

        return *this;
    }

    ImageWrapper::ImageWrapper(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::Format format,
        vk::Extent2D extent,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::ImageLayout initialLayout,
        vk::MemoryPropertyFlags memoryProperties,
        vk::ImageAspectFlags aspectMask)
        : m_format(format)
        , m_image(
            device,
            {vk::ImageCreateFlags(),
            vk::ImageType::e2D,
            format,
            vk::Extent3D(extent, 1),
            1,
            1,
            vk::SampleCountFlagBits::e1,
            tiling,
            usage | vk::ImageUsageFlagBits::eSampled,
            vk::SharingMode::eExclusive,
            {},
            initialLayout})
    {
        m_deviceMemory = allocateDeviceMemory(
            device,
            physicalDevice.getMemoryProperties(),
            m_image.getMemoryRequirements(),
            memoryProperties);
        m_image.bindMemory(m_deviceMemory, 0);
        m_imageView = vk::raii::ImageView(
            device,
            vk::ImageViewCreateInfo({}, m_image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, 1, 0, 1}));
    }

    ImageWrapper::ImageWrapper(ImageWrapper &&other) noexcept
        : m_format(other.m_format)
        , m_deviceMemory(std::move(other.m_deviceMemory))
        , m_image(std::move(other.m_image))
        , m_imageView(std::move(other.m_imageView))
    {
    }

    ImageWrapper const& ImageWrapper::operator=(ImageWrapper&& other)
    {
        if (this != &other)
        {
            m_format = other.m_format;
            m_deviceMemory = std::move(other.m_deviceMemory);
            m_image = std::move(other.m_image);
            m_imageView = std::move(other.m_imageView);
        }

        return *this;
    }

    DepthBufferWrapper::DepthBufferWrapper(vk::raii::PhysicalDevice const &physicalDevice,
                                           vk::raii::Device const &device,
                                           vk::Format format,
                                           vk::Extent2D const &extent)
        : ImageWrapper(
            physicalDevice,
            device,
            format,
            extent,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageLayout::eUndefined,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vk::ImageAspectFlagBits::eDepth)
    {
    }
    
    DepthBufferWrapper::DepthBufferWrapper(DepthBufferWrapper&& other) noexcept
        : ImageWrapper(std::move(other))
    {
    }

    DepthBufferWrapper const& DepthBufferWrapper::operator=(DepthBufferWrapper&& other)
    {
        if (this != &other)
        {
            m_format = other.m_format;
            m_deviceMemory = std::move(other.m_deviceMemory);
            m_image = std::move(other.m_image);
            m_imageView = std::move(other.m_imageView);
        }

        return *this;
    }

    TextureWrapper::TextureWrapper(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::Extent2D extent,
        vk::ImageUsageFlags usageFlags,
        vk::FormatFeatureFlags formatFeatureFlags,
        bool anisotropyEnable,
        bool forceStaging)
        : m_format(vk::Format::eR8G8B8A8Unorm)
        , m_extent(extent)
        , m_sampler(
            device,
            {{},
            vk::Filter::eLinear,
            vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            0.0f,
            anisotropyEnable,
            16.0f,
            false,
            vk::CompareOp::eNever,
            0.0f,
            0.0f,
            vk::BorderColor::eFloatOpaqueBlack})
    {
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(m_format);

        formatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImage;
        m_needsStaging = forceStaging || ((formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags);
        vk::ImageTiling imageTiling;
        vk::ImageLayout initialLayout;
        vk::MemoryPropertyFlags requirements;
        if (m_needsStaging)
        {
            assert((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
            m_stagingBuffer = std::move(BufferWrapper(physicalDevice, device, m_extent.width * m_extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc));
            imageTiling = vk::ImageTiling::eOptimal;
            usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
            initialLayout = vk::ImageLayout::eUndefined;
        }
        else
        {
            imageTiling = vk::ImageTiling::eLinear;
            initialLayout = vk::ImageLayout::ePreinitialized;
            requirements = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
        }
        m_image = std::move(ImageWrapper(
            physicalDevice,
            device,
            m_format,
            m_extent,
            imageTiling,
            usageFlags | vk::ImageUsageFlagBits::eSampled,
            initialLayout,
            requirements,
            vk::ImageAspectFlagBits::eColor));
    }

    TextureWrapper::TextureWrapper(TextureWrapper &&other) noexcept
        : m_format(other.m_format)
        , m_extent(other.m_extent)
        , m_needsStaging(other.m_needsStaging)
        , m_stagingBuffer(std::move(other.m_stagingBuffer))
        , m_image(std::move(other.m_image))
        , m_sampler(std::move(other.m_sampler))
    {
    }

    TextureWrapper const& TextureWrapper::operator=(TextureWrapper&& other)
    {
        if (this != &other)
        {
            m_format = other.m_format;
            m_extent = other.m_extent;
            m_needsStaging = other.m_needsStaging;
            m_stagingBuffer = std::move(other.m_stagingBuffer);
            m_image = std::move(other.m_image);
            m_sampler = std::move(other.m_sampler);
        }

        return *this;
    }

} // namespace spock
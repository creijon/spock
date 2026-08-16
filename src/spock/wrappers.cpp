#include "wrappers.hpp"

#include "helpers.hpp"

#include <iostream>

namespace spock
{
    BufferWrapper::BufferWrapper(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags)
        : buffer(device, vk::BufferCreateInfo({}, size, usage))
#if !defined(NDEBUG)
        , m_size(size)
        , m_usage(usage)
        , m_propertyFlags(propertyFlags)
#endif
    {
        deviceMemory = allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), buffer.getMemoryRequirements(), propertyFlags);
        buffer.bindMemory(deviceMemory, 0);
    }

    BufferWrapper::BufferWrapper(BufferWrapper &&other) noexcept
        : deviceMemory(std::move(other.deviceMemory))
        , buffer(std::move(other.buffer))
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
            deviceMemory = std::move(other.deviceMemory);
            buffer = std::move(other.buffer);
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
        vk::Format format_,
        vk::Extent2D const &extent,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::ImageLayout initialLayout,
        vk::MemoryPropertyFlags memoryProperties,
        vk::ImageAspectFlags aspectMask)
        : format(format_)
        , image(device,
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
        deviceMemory = allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), image.getMemoryRequirements(), memoryProperties);
        image.bindMemory(deviceMemory, 0);
        imageView = vk::raii::ImageView(device, vk::ImageViewCreateInfo({}, image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, 1, 0, 1}));
    }

    ImageWrapper::ImageWrapper(ImageWrapper &&other) noexcept
        : format(other.format)
        , deviceMemory(std::move(other.deviceMemory))
        , image(std::move(other.image))
        , imageView(std::move(other.imageView))
    {
    }

    ImageWrapper const& ImageWrapper::operator=(ImageWrapper&& other)
    {
        if (this != &other)
        {
            format = other.format;
            deviceMemory = std::move(other.deviceMemory);
            image = std::move(other.image);
            imageView = std::move(other.imageView);
        }

        return *this;
    }

    DepthBufferWrapper::DepthBufferWrapper(vk::raii::PhysicalDevice const &physicalDevice,
                                           vk::raii::Device const &device,
                                           vk::Format format,
                                           vk::Extent2D const &extent)
        : ImageWrapper(physicalDevice,
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
            format = other.format;
            deviceMemory = std::move(other.deviceMemory);
            image = std::move(other.image);
            imageView = std::move(other.imageView);
        }

        return *this;
    }

    TextureWrapper::TextureWrapper(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        vk::Extent2D const &extent_,
        vk::ImageUsageFlags usageFlags,
        vk::FormatFeatureFlags formatFeatureFlags,
        bool anisotropyEnable,
        bool forceStaging)
        : format(vk::Format::eR8G8B8A8Unorm)
        , extent(extent_)
        , sampler(device,
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
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);

        formatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImage;
        needsStaging = forceStaging || ((formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags);
        vk::ImageTiling imageTiling;
        vk::ImageLayout initialLayout;
        vk::MemoryPropertyFlags requirements;
        if (needsStaging)
        {
            assert((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
            stagingBuffer = std::move(BufferWrapper(physicalDevice, device, extent.width * extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc));
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
        image = std::move(ImageWrapper(physicalDevice,
                                       device,
                                       format,
                                       extent,
                                       imageTiling,
                                       usageFlags | vk::ImageUsageFlagBits::eSampled,
                                       initialLayout,
                                       requirements,
                                       vk::ImageAspectFlagBits::eColor));
    }

    TextureWrapper::TextureWrapper(TextureWrapper &&other) noexcept
        : format(other.format)
        , extent(other.extent)
        , needsStaging(other.needsStaging)
        , stagingBuffer(std::move(other.stagingBuffer))
        , image(std::move(other.image))
        , sampler(std::move(other.sampler))
    {
    }

    TextureWrapper const& TextureWrapper::operator=(TextureWrapper&& other)
    {
        if (this != &other)
        {
            format = other.format;
            extent = other.extent;
            needsStaging = other.needsStaging;
            stagingBuffer = std::move(other.stagingBuffer);
            image = std::move(other.image);
            sampler = std::move(other.sampler);
        }

        return *this;
    }

} // namespace spock
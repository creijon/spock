// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/helpers.hpp"

#include "spock/utils.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace spock
{
    // The stage and access mask that an earlier use of an image in `layout` has to be waited for.
    void sourceScope(
        vk::ImageLayout layout,
        vk::PipelineStageFlags& stage,
        vk::AccessFlags& access)
    {
        switch (layout)
        {
            case vk::ImageLayout::eUndefined:
                stage = vk::PipelineStageFlagBits::eTopOfPipe;
                access = {};
                break;
            case vk::ImageLayout::ePreinitialized:
                stage = vk::PipelineStageFlagBits::eHost;
                access = vk::AccessFlagBits::eHostWrite;
                break;
            case vk::ImageLayout::eGeneral:
                // Host, transfer and compute writes all use the general layout, so wait for any of them.
                stage = vk::PipelineStageFlagBits::eAllCommands | vk::PipelineStageFlagBits::eHost;
                access = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eHostWrite;
                break;
            case vk::ImageLayout::eTransferDstOptimal:
                stage = vk::PipelineStageFlagBits::eTransfer;
                access = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                // A read only needs an execution dependency.
                stage = vk::PipelineStageFlagBits::eTransfer;
                access = {};
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader;
                access = {};
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                access = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                stage = vk::PipelineStageFlagBits::eLateFragmentTests;
                access = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::ePresentSrcKHR:
                stage = vk::PipelineStageFlagBits::eBottomOfPipe;
                access = {};
                break;
            default:
                throw std::invalid_argument("setImageLayout: unsupported old layout " + vk::to_string(layout));
        }
    }

    // The stage and access mask that a later use of an image in `layout` has to wait in.
    void destinationScope(
        vk::ImageLayout layout,
        vk::PipelineStageFlags& stage,
        vk::AccessFlags& access)
    {
        switch (layout)
        {
            case vk::ImageLayout::eGeneral:
                stage = vk::PipelineStageFlagBits::eAllCommands | vk::PipelineStageFlagBits::eHost;
                access = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite;
                break;
            case vk::ImageLayout::eTransferDstOptimal:
                stage = vk::PipelineStageFlagBits::eTransfer;
                access = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                stage = vk::PipelineStageFlagBits::eTransfer;
                access = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader;
                access = vk::AccessFlagBits::eShaderRead;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                access = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
                access = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::ePresentSrcKHR:
                // Presentation is ordered by the semaphore, not by the barrier.
                stage = vk::PipelineStageFlagBits::eBottomOfPipe;
                access = {};
                break;
            default:
                throw std::invalid_argument("setImageLayout: unsupported new layout " + vk::to_string(layout));
        }
    }
    // Clamp the requested swapchain image count to the supported min/max range.
    uint32_t clampSurfaceImageCount(
        uint32_t desiredImageCount,
        uint32_t minImageCount,
        uint32_t maxImageCount)
    {
        uint32_t imageCount = std::max(desiredImageCount, minImageCount);
        // Some drivers report maxImageCount as 0, so only clamp to max if it is valid.
        if (maxImageCount > 0)
        {
            imageCount = std::min(imageCount, maxImageCount);
        }
        return imageCount;
    }

    // Record an image memory barrier to transition an image from one layout to another.
    void setImageLayout(
        vk::CommandBuffer const& commandBuffer,
        vk::Image image,
        vk::Format format,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout,
        uint32_t layerCount)
    {
        vk::PipelineStageFlags sourceStage;
        vk::AccessFlags sourceAccessMask;
        sourceScope(oldImageLayout, sourceStage, sourceAccessMask);

        vk::PipelineStageFlags destinationStage;
        vk::AccessFlags destinationAccessMask;
        destinationScope(newImageLayout, destinationStage, destinationAccessMask);

        // The aspect depends on the format, not the layouts: a depth image keeps its depth aspect when it
        // moves out of the attachment layout, for example to be sampled.
        vk::ImageAspectFlags aspectMask;
        switch (format)
        {
            case vk::Format::eD16Unorm:
            case vk::Format::eX8D24UnormPack32:
            case vk::Format::eD32Sfloat:
                aspectMask = vk::ImageAspectFlagBits::eDepth;
                break;
            case vk::Format::eS8Uint:
                aspectMask = vk::ImageAspectFlagBits::eStencil;
                break;
            case vk::Format::eD16UnormS8Uint:
            case vk::Format::eD24UnormS8Uint:
            case vk::Format::eD32SfloatS8Uint:
                aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                break;
            default:
                aspectMask = vk::ImageAspectFlagBits::eColor;
                break;
        }

        vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, layerCount);
        vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask, destinationAccessMask, oldImageLayout, newImageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, imageSubresourceRange);
        commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
    }
    
    // Find a suitable memory type index that satisfies the required property flags.
    uint32_t findMemoryType(
        vk::PhysicalDeviceMemoryProperties const& memoryProperties,
        uint32_t typeBits,
        vk::MemoryPropertyFlags requirementsMask)
    {
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            if ((typeBits & (1u << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
            {
                return i;
            }
        }

        std::ostringstream message;
        message << "findMemoryType: no memory type in bits 0x" << std::hex << typeBits << " has properties " << vk::to_string(requirementsMask);
        throw std::runtime_error(message.str());
    }

    vk::raii::DeviceMemory allocateDeviceMemory(
        vk::raii::Device const &device,
        vk::PhysicalDeviceMemoryProperties const &memoryProperties,
        vk::MemoryRequirements const &memoryRequirements,
        vk::MemoryPropertyFlags memoryPropertyFlags)
    {
        uint32_t memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
        vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
        return vk::raii::DeviceMemory(device, memoryAllocateInfo);
    }

    // Select the most suitable surface format from the available list, with a
    // preference for SRGB color space and common 8-bit RGBA formats.
    vk::SurfaceFormatKHR pickSurfaceFormat(
        std::vector<vk::SurfaceFormatKHR> const &formats)
    {
        assert(!formats.empty());
        vk::SurfaceFormatKHR pickedFormat = formats[0];
        if (formats.size() == 1)
        {
            if (formats[0].format == vk::Format::eUndefined)
            {
                pickedFormat.format = vk::Format::eB8G8R8A8Unorm;
                pickedFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            }
        }
        else
        {
            // request several formats, the first found will be used
            vk::Format requestedFormats[]{
                vk::Format::eB8G8R8A8Unorm,
                vk::Format::eR8G8B8A8Unorm,
                vk::Format::eB8G8R8Unorm,
                vk::Format::eR8G8B8Unorm};
            vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            for (size_t i = 0; i < sizeof(requestedFormats) / sizeof(requestedFormats[0]); i++)
            {
                vk::Format requestedFormat = requestedFormats[i];
                auto it = std::find_if(
                    formats.begin(),
                    formats.end(),
                    [requestedFormat, requestedColorSpace](vk::SurfaceFormatKHR const &f)
                    { return (f.format == requestedFormat) && (f.colorSpace == requestedColorSpace); });
                if (it != formats.end())
                {
                    pickedFormat = *it;
                    break;
                }
            }
        }
        assert(pickedFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
        return pickedFormat;
    }

    vk::PresentModeKHR pickPresentMode(
        std::vector<vk::PresentModeKHR> const& presentModes,
        std::vector<vk::PresentModeKHR> const& preferred)
    {
        for (vk::PresentModeKHR mode : preferred)
        {
            if (std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end())
            {
                return mode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::CompositeAlphaFlagBitsKHR pickCompositeAlpha(
        vk::CompositeAlphaFlagsKHR supported,
        std::vector<vk::CompositeAlphaFlagBitsKHR> const& preferred)
    {
        for (vk::CompositeAlphaFlagBitsKHR mode : preferred)
        {
            if (supported & mode)
            {
                return mode;
            }
        }

        vk::CompositeAlphaFlagBitsKHR const all[]{
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
            vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
            vk::CompositeAlphaFlagBitsKHR::eInherit};
        for (vk::CompositeAlphaFlagBitsKHR mode : all)
        {
            if (supported & mode)
            {
                return mode;
            }
        }

        // The specification requires at least one bit to be set.
        throw std::runtime_error("pickCompositeAlpha: the surface reports no composite alpha mode");
    }

    uint32_t texelSize(vk::Format format)
    {
        switch (format)
        {
            case vk::Format::eR8Unorm:
            case vk::Format::eR8Snorm:
            case vk::Format::eR8Uint:
            case vk::Format::eR8Srgb:
                return 1;
            case vk::Format::eR8G8Unorm:
            case vk::Format::eR8G8Snorm:
            case vk::Format::eR8G8Uint:
            case vk::Format::eR8G8Srgb:
            case vk::Format::eR16Unorm:
            case vk::Format::eR16Uint:
            case vk::Format::eR16Sfloat:
                return 2;
            case vk::Format::eR8G8B8A8Unorm:
            case vk::Format::eR8G8B8A8Snorm:
            case vk::Format::eR8G8B8A8Uint:
            case vk::Format::eR8G8B8A8Srgb:
            case vk::Format::eB8G8R8A8Unorm:
            case vk::Format::eB8G8R8A8Srgb:
            case vk::Format::eA2B10G10R10UnormPack32:
            case vk::Format::eA2R10G10B10UnormPack32:
            case vk::Format::eR16G16Unorm:
            case vk::Format::eR16G16Uint:
            case vk::Format::eR16G16Sfloat:
            case vk::Format::eR32Uint:
            case vk::Format::eR32Sfloat:
                return 4;
            case vk::Format::eR16G16B16A16Unorm:
            case vk::Format::eR16G16B16A16Uint:
            case vk::Format::eR16G16B16A16Sfloat:
            case vk::Format::eR32G32Uint:
            case vk::Format::eR32G32Sfloat:
                return 8;
            case vk::Format::eR32G32B32A32Uint:
            case vk::Format::eR32G32B32A32Sfloat:
                return 16;
            default:
                throw std::invalid_argument("texelSize: unsupported format " + vk::to_string(format));
        }
    }

    // Callback for Vulkan debug utils. Logs messages and filters out some
    // known non-actionable validation warnings in debug builds.
    VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
        [[maybe_unused]] void *pUserData)
    {
        (void)pUserData;
#if !defined(NDEBUG)
        switch (static_cast<uint32_t>(pCallbackData->messageIdNumber))
        {
        case 0:
            // Validation Warning: Override layer has override paths set to C:/VulkanSDK/<version>/Bin
            return vk::False;
        case 0x822806fa:
            // Validation Warning: vkCreateInstance(): to enable extension VK_EXT_debug_utils, but this extension is intended to support use by applications when
            // debugging and it is strongly recommended that it be otherwise avoided.
            return vk::False;
        case 0xe8d1a9fe:
            // Validation Performance Warning: Using debug builds of the validation layers *will* adversely affect performance.
            return vk::False;
        }
#endif

        std::cerr << vk::to_string(messageSeverity) << ": " << vk::to_string(messageTypes) << ":\n";
        std::cerr << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
        std::cerr << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
        std::cerr << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
        if (0 < pCallbackData->queueLabelCount)
        {
            std::cerr << std::string("\t") << "Queues Labels:\n";
            for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
            {
                std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
            }
        }
        if (0 < pCallbackData->cmdBufLabelCount)
        {
            std::cerr << std::string("\t") << "CommandBuffer Labels:\n";
            for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
            {
                std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
            }
        }
        if (0 < pCallbackData->objectCount)
        {
            std::cerr << std::string("\t") << "Objects:\n";
            for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
            {
                std::cerr << std::string("\t\t") << "Object " << i << "\n";
                std::cerr << std::string("\t\t\t") << "objectType   = " << vk::to_string(pCallbackData->pObjects[i].objectType) << "\n";
                std::cerr << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
                if (pCallbackData->pObjects[i].pObjectName)
                {
                    std::cerr << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
                }
            }
        }
        return vk::False;
    }

    vk::DebugUtilsMessengerCreateInfoEXT makeDebugUtilsMessengerCreateInfoEXT()
    {
        return {{},
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                &debugUtilsMessengerCallback};
    }
} // namespace spock
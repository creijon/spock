// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "helpers.hpp"

#include <iostream>
#include <sstream>
#include <string>

// Platform detection and headers
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#define PLATFORM_MACOS
#include <os/log.h>
#else
#define PLATFORM_LINUX
#include <cstdio>
#endif

namespace spock
{
    void writeLog(const std::string& message)
    {
#if defined(PLATFORM_WINDOWS)
        // Sends output directly to Visual Studio Debugger Output Window
        if (IsDebuggerPresent())
        {
            OutputDebugStringA(message.c_str());
        }
        else
        {
            std::fputs(message.c_str(), stderr);
            std::fflush(stderr);
        }
#elif defined(PLATFORM_MACOS)
        // Sends output to OS Log (viewable in Console.app)
        os_log_error(OS_LOG_DEFAULT, "%{public}s", message.c_str());
#else
        // Writes to stderr on Linux / Unix systems
        std::fputs(message.c_str(), stderr);
        std::fflush(stderr);
#endif
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
        vk::raii::CommandBuffer const &commandBuffer,
        vk::Image image,
        vk::Format format,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout)
    {
        vk::AccessFlags sourceAccessMask;
        switch (oldImageLayout)
        {
        case vk::ImageLayout::eTransferDstOptimal:
            sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::ePreinitialized:
            sourceAccessMask = vk::AccessFlagBits::eHostWrite;
            break;
        case vk::ImageLayout::eGeneral: // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined:
            break;
        default:
            assert(false);
            break;
        }

        vk::PipelineStageFlags sourceStage;
        switch (oldImageLayout)
        {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized:
            sourceStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eUndefined:
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        default:
            assert(false);
            break;
        }

        vk::AccessFlags destinationAccessMask;
        switch (newImageLayout)
        {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral: // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR:
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            assert(false);
            break;
        }

        vk::PipelineStageFlags destinationStage;
        switch (newImageLayout)
        {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            destinationStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::ePresentSrcKHR:
            destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        default:
            assert(false);
            break;
        }

        vk::ImageAspectFlags aspectMask;
        if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
        {
            aspectMask = vk::ImageAspectFlagBits::eDepth;
            if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
            {
                aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        }
        else
        {
            aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
        vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask,
                                                  destinationAccessMask,
                                                  oldImageLayout,
                                                  newImageLayout,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  image,
                                                  imageSubresourceRange);
        return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
    }

    // Find a suitable memory type index that satisfies the required property flags.
    uint32_t findMemoryType(
        vk::PhysicalDeviceMemoryProperties const &memoryProperties,
        uint32_t typeBits,
        vk::MemoryPropertyFlags requirementsMask)
    {
        uint32_t typeIndex = uint32_t(~0);
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
            {
                typeIndex = i;
                break;
            }
            typeBits >>= 1;
        }
        assert(typeIndex != uint32_t(~0));
        return typeIndex;
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
                auto it = std::find_if(formats.begin(),
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
        std::vector<vk::PresentModeKHR> const &presentModes)
    {
        vk::PresentModeKHR pickedMode = vk::PresentModeKHR::eFifo;
        for (const auto &presentMode : presentModes)
        {
            if (presentMode == vk::PresentModeKHR::eMailbox)
            {
                pickedMode = presentMode;
                break;
            }

            if (presentMode == vk::PresentModeKHR::eImmediate)
            {
                pickedMode = presentMode;
            }
        }
        return pickedMode;
    }

    // Find the first queue family index that supports graphics commands.
    uint32_t findGraphicsQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties)
    {
        // Get the first index that supports graphics.
        auto graphicsQueueFamilyProperty =
            std::find_if(queueFamilyProperties.begin(),
                         queueFamilyProperties.end(),
                         [](vk::QueueFamilyProperties const &qfp)
                         { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
        assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
        return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
    }

    // Find queue family indices for graphics and presentation. If a single queue
    // family supports both, return the same index for both graphics and present.
    QueueIndices findGraphicsAndPresentQueueFamilyIndex(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::SurfaceKHR const &surface)
    {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        uint32_t graphicsQueueFamilyIndex = findGraphicsQueueFamilyIndex(queueFamilyProperties);
        if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex, surface))
        {
            // The first graphicsQueueFamilyIndex also supports present.
            return {graphicsQueueFamilyIndex, graphicsQueueFamilyIndex};
        }

        // The graphicsQueueFamilyIndex doesn't support present, so look for
        // another family index that supports both graphics and present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return {i,i,};
            }
        }

        // There's nothing like a single family index that supports both graphics
        // and present, so look for another family index that supports present.
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return {graphicsQueueFamilyIndex, i};
            }
        }

        throw std::runtime_error("Could not find queues for both graphics or present -> terminating");
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
            std::cerr << std::string("\t") << "Queue Labels:\n";
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
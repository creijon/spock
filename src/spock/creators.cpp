// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "creators.hpp"
#include "utils.hpp"

#include <iostream>
#include <numeric>

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h>
#endif

namespace spock
{
    std::vector<std::string> getDefaultInstanceExtensions()
    {
        std::vector<std::string> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_VI_NN)
        extensions.push_back(VK_NN_VI_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
        extensions.push_back(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
#endif
        return extensions;
    }

    std::vector<std::string> getDefaultDeviceExtensions()
    {
        return {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        };
    }

    // Convert requested extension names into raw const char* pointers while
    // checking that they are available on the target implementation.
    std::vector<char const *> gatherExtensions(
        std::vector<std::string> const &extensions,
        std::vector<vk::ExtensionProperties> const &extensionProperties)
    {
        std::vector<char const *> enabledExtensions;

        enabledExtensions.reserve(extensions.size());

        for (auto const &ext : extensions)
        {
            assert(std::any_of(extensionProperties.begin(), extensionProperties.end(),
                               [ext](vk::ExtensionProperties const &ep)
                               {
                                   return ext == ep.extensionName;
                               }));
            enabledExtensions.push_back(ext.data());
        }

        if (std::none_of(extensions.begin(), extensions.end(),
                         [](std::string const &extension)
                         {
                             return extension == VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                         }) &&
            std::any_of(extensionProperties.begin(), extensionProperties.end(),
                        [](vk::ExtensionProperties const &ep)
                        {
                            return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0);
                        }))
        {
            enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return enabledExtensions;
    }

    // Convert requested validation layer names into raw const char* pointers
    // while checking that they exist in the available layer list.
    std::vector<char const *> gatherLayers(
        std::vector<std::string> const &layers,
        std::vector<vk::LayerProperties> const &layerProperties)
    {
        std::vector<char const *> enabledLayers;
        enabledLayers.reserve(layers.size());
        for (auto const &layer : layers)
        {
            assert(std::any_of(layerProperties.begin(), layerProperties.end(),
                               [layer](vk::LayerProperties const &lp)
                               {
                                   return layer == lp.layerName;
                               }));
            enabledLayers.push_back(layer.data());
        }
 
        // Enable standard validation layer to find as much errors as possible!
        if (std::none_of(layers.begin(), layers.end(),
                         [](std::string const &layer)
                         {
                             return layer == "VK_LAYER_KHRONOS_validation";
                         }) &&
            std::any_of(layerProperties.begin(), layerProperties.end(),
                        [](vk::LayerProperties const &lp)
                        {
                            return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0);
                        }))
        {
            enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
        }

        return enabledLayers;
    }

    // Create a Vulkan instance with the requested application name, layers,
    // extensions, and API version. In debug mode, also enable debug utils.
    vk::raii::Instance createInstance(
        vk::raii::Context const &context,
        std::string const &appName,
        std::vector<std::string> const &layers,
        std::vector<std::string> const &extensions,
        uint32_t apiVersion)
    {
        vk::ApplicationInfo applicationInfo(appName.c_str(), 1, "Spock", 1, apiVersion);

        vk::InstanceCreateFlags createFlags{};

#if defined(__APPLE__)
        createFlags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

#if defined(NDEBUG)
        std::vector<char const *> enabledLayers = gatherLayers(layers, {});
        std::vector<char const *> enabledExtensions =
            gatherExtensions(extensions, {});
        vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfoChain(
            {createFlags, &applicationInfo, enabledLayers, enabledExtensions});
#else
        std::vector<char const *> enabledLayers =
            gatherLayers(layers, context.enumerateInstanceLayerProperties());
        std::vector<char const *> enabledExtensions = gatherExtensions(
            extensions, context.enumerateInstanceExtensionProperties());

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::StructureChain<vk::InstanceCreateInfo,
                           vk::DebugUtilsMessengerCreateInfoEXT>
            instanceCreateInfoChain(
                {createFlags, &applicationInfo, enabledLayers, enabledExtensions},
                {{},
                 severityFlags,
                 messageTypeFlags,
                 &debugUtilsMessengerCallback});
#endif

        return vk::raii::Instance(
            context, instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
    }

    // Create a logical device for the selected physical device and queue family.
    // All requested extensions are enabled on the resulting device.
    vk::raii::Device createDevice(
        vk::raii::PhysicalDevice const &physicalDevice,
        uint32_t queueFamilyIndex,
        std::vector<std::string> const &extensions,
        vk::PhysicalDeviceFeatures const *physicalDeviceFeatures,
        void const *pNext)
    {
        std::vector<char const *> enabledExtensions;
        enabledExtensions.reserve(extensions.size());

        for (auto const &ext : extensions)
        {
            enabledExtensions.push_back(ext.data());
        }

        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority);
        vk::DeviceCreateInfo deviceCreateInfo(
            vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions,
            physicalDeviceFeatures, pNext);

        return vk::raii::Device(physicalDevice, deviceCreateInfo);
    }

    // Create a basic render pass with a single color attachment and an optional
    // depth attachment, using the provided clear/load/store operations.
    vk::raii::RenderPass createRenderPass(
        vk::raii::Device const &device,
        vk::Format colorFormat,
        vk::Format depthFormat,
        vk::AttachmentLoadOp loadOp,
        vk::ImageLayout colorFinalLayout)
    {
        std::vector<vk::AttachmentDescription> attachmentDescriptions;

        assert(colorFormat != vk::Format::eUndefined);
        attachmentDescriptions.emplace_back(
            vk::AttachmentDescriptionFlags(),
            colorFormat,
            vk::SampleCountFlagBits::e1,
            loadOp,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            colorFinalLayout);

        if (depthFormat != vk::Format::eUndefined)
        {
            attachmentDescriptions.emplace_back(
                vk::AttachmentDescriptionFlags(),
                depthFormat,
                vk::SampleCountFlagBits::e1,
                loadOp,
                vk::AttachmentStoreOp::eDontCare,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal);
        }

        vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depthAttachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::SubpassDescription subpassDescription(
            vk::SubpassDescriptionFlags(),
            vk::PipelineBindPoint::eGraphics,
            {},
            colorAttachment,
            {},
            (depthFormat != vk::Format::eUndefined) ? &depthAttachment : nullptr);
        vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);

        return vk::raii::RenderPass(device, renderPassCreateInfo);
    }

    // Allocate and return a primary command buffer from the given command pool.
    vk::raii::CommandBuffer createCommandBuffer(
        vk::raii::Device const &device,
        vk::raii::CommandPool const &commandPool)
    {
        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            1);
        return std::move(vk::raii::CommandBuffers(device, commandBufferAllocateInfo).front());
    }

    // Create a descriptor pool with enough descriptor sets to cover the provided pool sizes.
    vk::raii::DescriptorPool createDescriptorPool(
        vk::raii::Device const &device,
        std::vector<vk::DescriptorPoolSize> const &poolSizes)
    {
        assert(!poolSizes.empty());
        uint32_t maxSets = std::accumulate(
            poolSizes.begin(), poolSizes.end(), 0, [](uint32_t sum, vk::DescriptorPoolSize const &dps)
            { return sum + dps.descriptorCount; });
        assert(0 < maxSets);

        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxSets, poolSizes);
        return vk::raii::DescriptorPool(device, descriptorPoolCreateInfo);
    }

    // Build a descriptor set layout from binding metadata describing descriptor types,
    // element counts and shader stage visibility.
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(
        vk::raii::Device const &device,
        std::vector<BindingData> const &bindingDatas,
        vk::DescriptorSetLayoutCreateFlags flags)
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        uint32_t index = 0;
        for (const auto& bindingData : bindingDatas)
        {
            bindings.emplace_back(index, bindingData.type, bindingData.count, bindingData.stageFlags);
            index++;
        }
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(flags, bindings);
        return vk::raii::DescriptorSetLayout(device, descriptorSetLayoutCreateInfo);
    }

    std::vector<vk::raii::Framebuffer> createFramebuffers(
        vk::raii::Device const &device,
        vk::raii::RenderPass &renderPass,
        std::vector<vk::raii::ImageView> const &imageViews,
        vk::raii::ImageView const *depthImageView,
        vk::Extent2D const &extent)
    {
        vk::ImageView attachments[2];
        attachments[1] = (depthImageView) ? *depthImageView : vk::ImageView();

        vk::FramebufferCreateInfo framebufferCreateInfo(
            vk::FramebufferCreateFlags(), renderPass, depthImageView ? 2 : 1, attachments, extent.width, extent.height, 1);
        std::vector<vk::raii::Framebuffer> framebuffers;
        framebuffers.reserve(imageViews.size());
        for (auto const &imageView : imageViews)
        {
            attachments[0] = imageView;
            framebuffers.push_back(vk::raii::Framebuffer(device, framebufferCreateInfo));
        }

        return framebuffers;
    }

    // Create a graphics pipeline configured for the given vertex format,
    // shaders, render pass, and optional depth testing.
    vk::raii::Pipeline createGraphicsPipeline(
        vk::raii::Device const &device,
        std::vector<vk::PipelineShaderStageCreateInfo> const &shaderStagesInfo,
        vk::raii::PipelineLayout const &pipelineLayout,
        vk::raii::RenderPass const &renderPass,
        VertexFormat const& vertexFormat,
        vk::PrimitiveTopology primitiveTopology,
        vk::CullModeFlagBits cullMode,
        bool depthBuffered)
    {
        std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions;

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
            vk::PipelineInputAssemblyStateCreateFlags(),
            primitiveTopology);

        vk::PipelineViewportStateCreateInfo viewportInfo(
            vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

        vk::PipelineRasterizationStateCreateInfo rasterizationInfo(
            vk::PipelineRasterizationStateCreateFlags(),
            false,
            false,
            vk::PolygonMode::eFill,
            cullMode,
            vk::FrontFace::eClockwise,
            false,
            0.0f,
            0.0f,
            0.0f,
            1.0f);

        vk::PipelineMultisampleStateCreateInfo multisampleInfo({}, vk::SampleCountFlagBits::e1);

        vk::StencilOpState stencilOpState(
            vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways);
        vk::PipelineDepthStencilStateCreateInfo depthStencilInfo(
            vk::PipelineDepthStencilStateCreateFlags(),
            depthBuffered,
            depthBuffered,
            vk::CompareOp::eLessOrEqual,
            false,
            false,
            stencilOpState,
            stencilOpState);

        vk::ColorComponentFlags colorComponentFlags(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachment(
            true,
            vk::BlendFactor::eSrcAlpha,
            vk::BlendFactor::eOneMinusSrcAlpha,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            colorComponentFlags);
        vk::PipelineColorBlendStateCreateInfo colorBlendInfo(
            vk::PipelineColorBlendStateCreateFlags(),
            false,
            vk::LogicOp::eNoOp,
            pipelineColorBlendAttachment,
            {{1.0f, 1.0f, 1.0f, 1.0f}});

        std::array<vk::DynamicState, 2> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicStateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{vertexFormat};

        vk::GraphicsPipelineCreateInfo graphicsPipelineInfo(
            vk::PipelineCreateFlags(),
            shaderStagesInfo,
            &vertexInputInfo,
            &inputAssemblyInfo,
            nullptr,
            &viewportInfo,
            &rasterizationInfo,
            &multisampleInfo,
            &depthStencilInfo,
            &colorBlendInfo,
            &dynamicStateInfo,
            pipelineLayout,
            renderPass);

        // TODO: might need to pass in VkPipelineCacheCreateFlagBits at some stage.
        vk::raii::PipelineCache cache(device, vk::PipelineCacheCreateInfo());

        return vk::raii::Pipeline(device, cache, graphicsPipelineInfo);
    }

    // Upload buffer and texture bindings into the descriptor set. Buffer data is
    // placed sequentially starting at bindingOffset followed by any textures.
    void updateDescriptorSets(
        vk::raii::Device const &device,
        vk::raii::DescriptorSet const &descriptorSet,
        std::vector<BufferUpdateData> const& bufferData,
        std::vector<TextureWrapper> const &textureData,
        uint32_t bindingOffset)
    {
        std::vector<vk::DescriptorBufferInfo> bufferInfos;
        bufferInfos.reserve(bufferData.size());

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
        writeDescriptorSets.reserve(bufferData.size() + (textureData.empty() ? 0 : 1));
        uint32_t dstBinding = bindingOffset;
        for (auto const &bd : bufferData)
        {
            bufferInfos.emplace_back(bd.buffer, 0, bd.size);
            vk::BufferView bufferView{nullptr};
            if (bd.bufferView)
            {
                bufferView = *bd.bufferView;
            }
            writeDescriptorSets.emplace_back(
                descriptorSet, 
                dstBinding++,
                0,
                1,
                bd.type,
                nullptr,
                &bufferInfos.back(),
                bd.bufferView ? &bufferView : nullptr);
        }

        std::vector<vk::DescriptorImageInfo> imageInfos;
        if (!textureData.empty())
        {
            imageInfos.reserve(textureData.size());
            for (auto const &thd : textureData)
            {
                imageInfos.emplace_back(
                    thd.sampler(),
                    thd.image().imageView(),
                    vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            writeDescriptorSets.emplace_back(
                descriptorSet,
                dstBinding,
                0,
                checked_cast<uint32_t>(imageInfos.size()),
                vk::DescriptorType::eCombinedImageSampler,
                imageInfos.data(),
                nullptr,
                nullptr);
        }

        device.updateDescriptorSets(writeDescriptorSets, nullptr);
    }

} // namespace spock

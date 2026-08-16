#include "creators.hpp"

#include <iostream>
#include <numeric>

namespace spock
{
    std::vector<std::string> getInstanceExtensions()
    {
        std::vector<std::string> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
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

    // Return the device extensions required by this sample, currently only swapchain support.
    std::vector<std::string> getDeviceExtensions()
    {
        return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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

    GLFWwindow* createWindow(
        std::string const& windowName,
        vk::Extent2D const& extent)
    {
        struct glfwContext
        {
            glfwContext()
            {
                glfwInit();
                glfwSetErrorCallback([](int error, const char* msg)
                    { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; });
            }

            ~glfwContext()
            {
                glfwTerminate();
            }
        };

        static auto glfwCtx = glfwContext();
        (void)glfwCtx;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        return glfwCreateWindow(extent.width, extent.height, windowName.c_str(), nullptr, nullptr);
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

#if defined(NDEBUG)
        std::vector<char const *> enabledLayers = gatherLayers(layers, {});
        std::vector<char const *> enabledExtensions =
            gatherExtensions(extensions, {});
        vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfoChain(
            {{}, &applicationInfo, enabledLayers, enabledExtensions});
#else
        std::vector<char const *> enabledLayers =
            gatherLayers(layers, context.enumerateInstanceLayerProperties());
        std::vector<char const *> enabledExtensions = gatherExtensions(
            extensions, context.enumerateInstanceExtensionProperties());

        // in debug mode, addionally use the debugUtilsMessengerCallback in instance
        // creation!
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
                {{}, &applicationInfo, enabledLayers, enabledExtensions},
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
        attachmentDescriptions.emplace_back(vk::AttachmentDescriptionFlags(),
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
            attachmentDescriptions.emplace_back(vk::AttachmentDescriptionFlags(),
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

        vk::SubpassDescription subpassDescription(vk::SubpassDescriptionFlags(),
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
        std::vector<std::tuple<vk::DescriptorType,
                               uint32_t, vk::ShaderStageFlags>> const &bindingData,
        vk::DescriptorSetLayoutCreateFlags flags)
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings(bindingData.size());
        for (size_t i = 0; i < bindingData.size(); i++)
        {
            bindings[i] = vk::DescriptorSetLayoutBinding(
                checked_cast<uint32_t>(i), std::get<0>(bindingData[i]), std::get<1>(bindingData[i]), std::get<2>(bindingData[i]));
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
        vk::raii::PipelineCache const &pipelineCache,
        vk::raii::ShaderModule const &vertexShaderModule,
        vk::SpecializationInfo const *vertexShaderSpecializationInfo,
        vk::raii::ShaderModule const &fragmentShaderModule,
        vk::SpecializationInfo const *fragmentShaderSpecializationInfo,
        uint32_t vertexStride,
        std::vector<std::pair<vk::Format, uint32_t>> const &vertexInputAttributeFormatOffset,
        vk::FrontFace frontFace,
        bool depthBuffered,
        vk::raii::PipelineLayout const &pipelineLayout,
        vk::raii::RenderPass const &renderPass)
    {
        std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main", vertexShaderSpecializationInfo),
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", fragmentShaderSpecializationInfo)};

        std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
        vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
        vk::VertexInputBindingDescription vertexInputBindingDescription(0, vertexStride);

        if (0 < vertexStride)
        {
            vertexInputAttributeDescriptions.reserve(vertexInputAttributeFormatOffset.size());
            for (uint32_t i = 0; i < vertexInputAttributeFormatOffset.size(); i++)
            {
                vertexInputAttributeDescriptions.emplace_back(i, 0, vertexInputAttributeFormatOffset[i].first, vertexInputAttributeFormatOffset[i].second);
            }
            pipelineVertexInputStateCreateInfo.setVertexBindingDescriptions(vertexInputBindingDescription);
            pipelineVertexInputStateCreateInfo.setVertexAttributeDescriptions(vertexInputAttributeDescriptions);
        }

        vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
                                                                                      vk::PrimitiveTopology::eTriangleList);

        vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

        vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(vk::PipelineRasterizationStateCreateFlags(),
                                                                                      false,
                                                                                      false,
                                                                                      vk::PolygonMode::eFill,
                                                                                      vk::CullModeFlagBits::eBack,
                                                                                      frontFace,
                                                                                      false,
                                                                                      0.0f,
                                                                                      0.0f,
                                                                                      0.0f,
                                                                                      1.0f);

        vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);

        vk::StencilOpState stencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways);
        vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
            vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered, vk::CompareOp::eLessOrEqual, false, false, stencilOpState, stencilOpState);

        vk::ColorComponentFlags colorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                                                    vk::ColorComponentFlagBits::eA);
        vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(false,
                                                                                vk::BlendFactor::eZero,
                                                                                vk::BlendFactor::eZero,
                                                                                vk::BlendOp::eAdd,
                                                                                vk::BlendFactor::eZero,
                                                                                vk::BlendFactor::eZero,
                                                                                vk::BlendOp::eAdd,
                                                                                colorComponentFlags);
        vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
            vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp, pipelineColorBlendAttachmentState, {{1.0f, 1.0f, 1.0f, 1.0f}});

        std::array<vk::DynamicState, 2> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(vk::PipelineCreateFlags(),
                                                                  pipelineShaderStageCreateInfos,
                                                                  &pipelineVertexInputStateCreateInfo,
                                                                  &pipelineInputAssemblyStateCreateInfo,
                                                                  nullptr,
                                                                  &pipelineViewportStateCreateInfo,
                                                                  &pipelineRasterizationStateCreateInfo,
                                                                  &pipelineMultisampleStateCreateInfo,
                                                                  &pipelineDepthStencilStateCreateInfo,
                                                                  &pipelineColorBlendStateCreateInfo,
                                                                  &pipelineDynamicStateCreateInfo,
                                                                  pipelineLayout,
                                                                  renderPass);

        return vk::raii::Pipeline(device, pipelineCache, graphicsPipelineCreateInfo);
    }

    // Upload buffer and texture bindings into the descriptor set. Buffer data is
    // placed sequentially starting at bindingOffset followed by any textures.
    void updateDescriptorSets(
        vk::raii::Device const &device,
        vk::raii::DescriptorSet const &descriptorSet,
        DescriptorSetUpdateData const &bufferData,
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
            bufferInfos.emplace_back(std::get<1>(bd), 0, std::get<2>(bd));
            vk::BufferView bufferView;
            if (std::get<3>(bd))
            {
                bufferView = *std::get<3>(bd);
            }
            writeDescriptorSets.emplace_back(
                descriptorSet, dstBinding++, 0, 1, std::get<0>(bd), nullptr, &bufferInfos.back(), std::get<3>(bd) ? &bufferView : nullptr);
        }

        std::vector<vk::DescriptorImageInfo> imageInfos;
        if (!textureData.empty())
        {
            imageInfos.reserve(textureData.size());
            for (auto const &thd : textureData)
            {
                imageInfos.emplace_back(thd.sampler, thd.image.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            writeDescriptorSets.emplace_back(descriptorSet,
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

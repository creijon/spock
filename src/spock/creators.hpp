// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <glslang/Public/ShaderLang.h>

#include <string>
#include <vector>

namespace spock
{
    std::vector<std::string> getDefaultDeviceExtensions();
    std::vector<std::string> getDefaultInstanceExtensions();

    // Create a Vulkan instance with optional validation layers and requested
    // extensions.
    vk::raii::Instance createInstance(
        vk::raii::Context const &context,
        std::string const &appName,
        std::vector<std::string> const &layers = {},
        std::vector<std::string> const &extensions = {},
        uint32_t apiVersion = VK_API_VERSION_1_0);

    // Create a logical Vulkan device and enable the requested device extensions.
    vk::raii::Device createDevice(
        vk::raii::PhysicalDevice const &physicalDevice,
        uint32_t queueFamilyIndex,
        std::vector<std::string> const &extensions = {},
        vk::PhysicalDeviceFeatures const *physicalDeviceFeatures = nullptr,
        void const *pNext = nullptr);

    // Allocate a primary command buffer from the given command pool.
    vk::raii::CommandBuffer createCommandBuffer(
        vk::raii::Device const &device,
        vk::raii::CommandPool const &commandPool);

    // Create a simple render pass that supports color and optional depth attachments.
    vk::raii::RenderPass createRenderPass(
        vk::raii::Device const &device,
        vk::Format colorFormat,
        vk::Format depthFormat,
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear,
        vk::ImageLayout colorFinalLayout = vk::ImageLayout::ePresentSrcKHR);

    // Create a descriptor pool that can allocate descriptor sets for the given bindings.
    vk::raii::DescriptorPool createDescriptorPool(
        vk::raii::Device const &device,
        std::vector<vk::DescriptorPoolSize> const &poolSizes);

    // Create a descriptor set layout from a list of binding descriptors.
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(
        vk::raii::Device const &device,
        std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>> const &bindingData,
        vk::DescriptorSetLayoutCreateFlags flags = {});

    // Create framebuffer objects for every swapchain image view and optional depth image view.
    std::vector<vk::raii::Framebuffer> createFramebuffers(
        vk::raii::Device const &device,
        vk::raii::RenderPass &renderPass,
        std::vector<vk::raii::ImageView> const &imageViews,
        vk::raii::ImageView const *pDepthImageView,
        vk::Extent2D const &extent);

    // Create a graphics pipeline using the provided shaders, vertex inputs, and render pass.
    vk::raii::Pipeline createGraphicsPipeline(
        vk::raii::Device const &device,
        std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStagesInfo,
        vk::raii::PipelineLayout const &pipelineLayout,
        vk::raii::RenderPass const &renderPass,
        VertexFormat const &vertexFormat,
        vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList,
        vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack,
        bool depthBuffered = true);

    // Typed alias for descriptor set update data. Each tuple contains the
    // descriptor type, buffer, buffer size, and optional buffer view.
    typedef std::vector<
        std::tuple<vk::DescriptorType,
                   vk::raii::Buffer const &,
                   vk::DeviceSize,
                   vk::raii::BufferView const *>>
        DescriptorSetUpdateData;

    // Update a descriptor set with uniform buffer bindings and optional textures.
    void updateDescriptorSets(
        vk::raii::Device const &device,
        vk::raii::DescriptorSet const &descriptorSet,
        DescriptorSetUpdateData const &bufferData,
        std::vector<TextureWrapper> const &textureData,
        uint32_t bindingOffset = 0);
} // namespace spock

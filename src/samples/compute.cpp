// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample is for the compute shader scaffolding.

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace
{
    constexpr uint32_t ELEMENT_COUNT = 1024;
    constexpr uint32_t WORKGROUP_SIZE = 64;

    const std::string COMPUTE_SHADER_SOURCE = R"(
#version 450

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer OutputBuffer
{
    uint values[];
};

layout(push_constant) uniform PushConstants
{
    uint count;
} pc;

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= pc.count)
    {
        return;
    }

    values[idx] = idx * idx;
}
)";

    struct PushConstants
    {
        uint32_t count;
    };
} // namespace

class ComputeRenderer : public spock::Renderer
{
public:
    ComputeRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.05f, 0.05f, 0.05f, 1.0f},
            {1.0f, 0})
    {
        createResources();
        createPipeline();
        runComputePass();
    }

protected:
    void createResources()
    {
        spock::BindingData outputBinding{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
        m_descriptorSetLayout = spock::createDescriptorSetLayout(m_device, {outputBinding});

        vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants)};
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, *m_descriptorSetLayout, pushConstantRange}));

        // Written by the compute shader; never touched by the host directly.
        m_storageBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            ELEMENT_COUNT * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Destination for a device-to-host copy so the result can be validated on the CPU.
        m_readbackBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            ELEMENT_COUNT * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        m_descriptorPool = spock::createDescriptorPool(m_device, {{vk::DescriptorType::eStorageBuffer, 1}});
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, {m_descriptorPool, *m_descriptorSetLayout}).front());

        spock::updateDescriptorSets(
            m_device,
            m_descriptorSet,
            {{vk::DescriptorType::eStorageBuffer, m_storageBuffer.buffer(), VK_WHOLE_SIZE, nullptr}},
            {});
    }

    void createPipeline()
    {
        glslang::InitializeProcess();
        vk::raii::ShaderModule computeShader{nullptr};
        try
        {
            computeShader = spock::compileShader(m_device, vk::ShaderStageFlagBits::eCompute, COMPUTE_SHADER_SOURCE);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        vk::PipelineShaderStageCreateInfo shaderStageInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eCompute,
            *computeShader,
            "main");

        m_computePipeline = spock::createComputePipeline(m_device, shaderStageInfo, m_pipelineLayout);
    }

    void runComputePass()
    {
        vk::DescriptorSet descriptorSet = *m_descriptorSet;
        vk::PipelineLayout pipelineLayout = *m_pipelineLayout;
        vk::Pipeline computePipeline = *m_computePipeline;
        vk::Buffer storageBuffer = *m_storageBuffer.buffer();
        vk::Buffer readbackBuffer = *m_readbackBuffer.buffer();

        const uint32_t groupCount = (ELEMENT_COUNT + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        const vk::DeviceSize bufferSize = ELEMENT_COUNT * sizeof(uint32_t);

        spock::oneTimeSubmit(
            m_device,
            m_commandPool,
            m_presenter->graphicsQueue(),
            [&](vk::CommandBuffer const &commandBuffer)
            {
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, {descriptorSet}, nullptr);

                PushConstants pushConstants{ELEMENT_COUNT};
                commandBuffer.pushConstants<PushConstants>(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pushConstants);

                commandBuffer.dispatch(groupCount, 1, 1);

                // The copy below must wait for the compute shader's writes to become visible.
                vk::MemoryBarrier memoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::DependencyFlags(),
                    memoryBarrier,
                    nullptr,
                    nullptr);

                commandBuffer.copyBuffer(storageBuffer, readbackBuffer, vk::BufferCopy(0, 0, bufferSize));
            });

        uint32_t const* results = static_cast<uint32_t const*>(m_readbackBuffer.map());
        bool allMatch = true;
        for (uint32_t i = 0; i < ELEMENT_COUNT; ++i)
        {
            if (results[i] != i * i)
            {
                allMatch = false;
                break;
            }
        }
        m_readbackBuffer.unmap();

        const std::string message = allMatch
            ? "Compute pass produced expected results for all " + std::to_string(ELEMENT_COUNT) + " elements.\n"
            : "Compute pass FAILED validation.\n";
        spock::writeLog(message);
        std::cout << message;
    }

    // Nothing is drawn; this sample only exercises the compute dispatch path above.
    void render(vk::raii::CommandBuffer const &, std::chrono::microseconds) override
    {
    }

private:
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_computePipeline{nullptr};

    spock::BufferWrapper m_storageBuffer;
    spock::BufferWrapper m_readbackBuffer;
};

class ComputeApp : public spock::App
{
public:
    ComputeApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Compute",
            windowWidth,
            windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<ComputeRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
    }
};

int main()
{
    return spock::runApp<ComputeApp>(400, 300);
}

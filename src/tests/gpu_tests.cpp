#include "gpu_fixture.hpp"

#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

using namespace spock_test;

TEST_CASE("a headless Vulkan device can be created for GPU-backed tests", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    CHECK(*fixture->device != VK_NULL_HANDLE);
}

TEST_CASE("findGraphicsAndPresentQueueFamilyIndex returns valid queue family indices", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    auto queueFamilyProperties = fixture->physicalDevice.getQueueFamilyProperties();
    CHECK(fixture->queueIndices.graphics < queueFamilyProperties.size());
    CHECK(fixture->queueIndices.present < queueFamilyProperties.size());
    CHECK((queueFamilyProperties[fixture->queueIndices.graphics].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics);
}

TEST_CASE("allocateDeviceMemory satisfies a real buffer's memory requirements", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::Buffer buffer(fixture->device, vk::BufferCreateInfo({}, 256, vk::BufferUsageFlagBits::eUniformBuffer));

    vk::raii::DeviceMemory memory = spock::allocateDeviceMemory(
        fixture->device,
        fixture->physicalDevice.getMemoryProperties(),
        buffer.getMemoryRequirements(),
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    CHECK(*memory != VK_NULL_HANDLE);
    CHECK_NOTHROW(buffer.bindMemory(memory, 0));
}

TEST_CASE("BufferWrapper uploads round-trip through mapped device memory", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    struct Uniforms
    {
        float values[4];
    };

    spock::BufferWrapper buffer(
        fixture->physicalDevice,
        fixture->device,
        sizeof(Uniforms),
        vk::BufferUsageFlagBits::eUniformBuffer);

    Uniforms written{{1.0f, 2.0f, 3.0f, 4.0f}};
    buffer.upload(written);

    Uniforms readBack{};
    void *mapped = buffer.deviceMemory().mapMemory(0, sizeof(Uniforms));
    std::memcpy(&readBack, mapped, sizeof(Uniforms));
    buffer.deviceMemory().unmapMemory();

    CHECK(readBack.values[0] == 1.0f);
    CHECK(readBack.values[1] == 2.0f);
    CHECK(readBack.values[2] == 3.0f);
    CHECK(readBack.values[3] == 4.0f);
}

TEST_CASE("BufferWrapper uploads a vector of elements", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    std::vector<uint32_t> written{10, 20, 30, 40, 50};

    spock::BufferWrapper buffer(
        fixture->physicalDevice,
        fixture->device,
        written.size() * sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer);

    buffer.upload(written);

    std::vector<uint32_t> readBack(written.size());
    void *mapped = buffer.deviceMemory().mapMemory(0, written.size() * sizeof(uint32_t));
    std::memcpy(readBack.data(), mapped, written.size() * sizeof(uint32_t));
    buffer.deviceMemory().unmapMemory();

    CHECK(readBack == written);
}

TEST_CASE("DepthBufferWrapper creates a bound image, memory and image view", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::DepthBufferWrapper depthBuffer(
        fixture->physicalDevice,
        fixture->device,
        vk::Format::eD16Unorm,
        vk::Extent2D(64, 64));

    CHECK(depthBuffer.format() == vk::Format::eD16Unorm);
    CHECK(*depthBuffer.image() != VK_NULL_HANDLE);
    CHECK(*depthBuffer.imageView() != VK_NULL_HANDLE);
    CHECK(*depthBuffer.deviceMemory() != VK_NULL_HANDLE);
}

TEST_CASE("TextureWrapper constructs and accepts image data via setImage", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    spock::TextureWrapper texture(fixture->physicalDevice, fixture->device, vk::Extent2D(4, 4));
    CHECK(*texture.sampler() != VK_NULL_HANDLE);

    vk::raii::CommandPool commandPool(
        fixture->device,
        vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, fixture->queueIndices.graphics));
    vk::raii::CommandBuffer commandBuffer = spock::createCommandBuffer(fixture->device, commandPool);
    vk::raii::Queue graphicsQueue(fixture->device, fixture->queueIndices.graphics, 0);

    // setImage() takes a vk::raii::CommandBuffer, so it's recorded and
    // submitted by hand here rather than through spock::oneTimeSubmit (which
    // hands its callback a plain, non-RAII vk::CommandBuffer).
    commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    texture.setImage(
        commandBuffer,
        [](void *data, vk::Extent2D const &extent)
        {
            std::memset(data, 0xFF, static_cast<size_t>(extent.width) * extent.height * 4);
        });
    commandBuffer.end();

    vk::CommandBuffer rawCommandBuffer = *commandBuffer;
    graphicsQueue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &rawCommandBuffer), nullptr);
    graphicsQueue.waitIdle();
}

TEST_CASE("compileShader produces a usable shader module from valid GLSL", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    static const std::string vertexSource = R"(
#version 450
layout(location = 0) in vec4 pos;
void main() { gl_Position = pos; }
)";

    vk::raii::ShaderModule module = spock::compileShader(fixture->device, vk::ShaderStageFlagBits::eVertex, vertexSource);

    CHECK(*module != VK_NULL_HANDLE);
}

TEST_CASE("createRenderPass and createFramebuffers build a color+depth render target", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Extent2D extent(64, 64);
    vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;

    vk::raii::Image colorImage(
        fixture->device,
        vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            colorFormat,
            vk::Extent3D(extent, 1),
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment));
    vk::raii::DeviceMemory colorMemory = spock::allocateDeviceMemory(
        fixture->device,
        fixture->physicalDevice.getMemoryProperties(),
        colorImage.getMemoryRequirements(),
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    colorImage.bindMemory(colorMemory, 0);

    std::vector<vk::raii::ImageView> colorImageViews;
    colorImageViews.emplace_back(
        fixture->device,
        vk::ImageViewCreateInfo({}, colorImage, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}));

    spock::DepthBufferWrapper depthBuffer(fixture->physicalDevice, fixture->device, vk::Format::eD16Unorm, extent);

    vk::raii::RenderPass renderPass = spock::createRenderPass(fixture->device, colorFormat, depthBuffer.format());
    CHECK(*renderPass != VK_NULL_HANDLE);

    std::vector<vk::raii::Framebuffer> framebuffers = spock::createFramebuffers(
        fixture->device, renderPass, colorImageViews, &depthBuffer.imageView(), extent);

    REQUIRE(framebuffers.size() == 1);
    CHECK(*framebuffers[0] != VK_NULL_HANDLE);
}

TEST_CASE("createCommandBuffer allocates a primary command buffer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::CommandPool commandPool(
        fixture->device,
        vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, fixture->queueIndices.graphics));

    vk::raii::CommandBuffer commandBuffer = spock::createCommandBuffer(fixture->device, commandPool);

    CHECK(*commandBuffer != VK_NULL_HANDLE);
    CHECK_NOTHROW(commandBuffer.begin(vk::CommandBufferBeginInfo()));
    CHECK_NOTHROW(commandBuffer.end());
}

TEST_CASE("createDescriptorSetLayout, createDescriptorPool and updateDescriptorSets wire up a uniform buffer binding", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::DescriptorSetLayout descriptorSetLayout = spock::createDescriptorSetLayout(
        fixture->device,
        {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
    CHECK(*descriptorSetLayout != VK_NULL_HANDLE);

    vk::raii::DescriptorPool descriptorPool = spock::createDescriptorPool(
        fixture->device,
        {{vk::DescriptorType::eUniformBuffer, 1}});
    CHECK(*descriptorPool != VK_NULL_HANDLE);

    vk::raii::DescriptorSets descriptorSets(
        fixture->device, vk::DescriptorSetAllocateInfo(descriptorPool, *descriptorSetLayout));
    vk::raii::DescriptorSet descriptorSet = std::move(descriptorSets.front());

    spock::BufferWrapper uniformBuffer(
        fixture->physicalDevice, fixture->device, sizeof(float) * 16, vk::BufferUsageFlagBits::eUniformBuffer);

    spock::DescriptorSetUpdateData bufferData{
        {vk::DescriptorType::eUniformBuffer, uniformBuffer.buffer(), sizeof(float) * 16, nullptr}};

    CHECK_NOTHROW(spock::updateDescriptorSets(fixture->device, descriptorSet, bufferData, {}));
}

TEST_CASE("createGraphicsPipeline builds a pipeline from compiled shaders and a push-constant layout", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    static const std::string vertexSource = R"(
#version 450
layout(push_constant) uniform PushConstants { mat4 mvp; } pc;
layout(location = 0) in vec4 pos;
void main() { gl_Position = pc.mvp * pos; }
)";
    static const std::string fragmentSource = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0); }
)";

    vk::raii::ShaderModule vertexModule = spock::compileShader(fixture->device, vk::ShaderStageFlagBits::eVertex, vertexSource);
    vk::raii::ShaderModule fragmentModule = spock::compileShader(fixture->device, vk::ShaderStageFlagBits::eFragment, fragmentSource);

    vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;
    vk::raii::RenderPass renderPass = spock::createRenderPass(fixture->device, colorFormat, vk::Format::eUndefined);

    vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 16);
    vk::raii::PipelineLayout pipelineLayout(fixture->device, vk::PipelineLayoutCreateInfo({}, {}, pushConstantRange));

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo = {
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertexModule, "main"},
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragmentModule, "main"}};

    vk::raii::Pipeline pipeline = spock::createGraphicsPipeline(
        fixture->device,
        shaderStagesInfo,
        sizeof(float) * 4,
        {{vk::Format::eR32G32B32A32Sfloat, 0}},
        vk::PrimitiveTopology::eTriangleList,
        vk::FrontFace::eClockwise,
        false,
        pipelineLayout,
        renderPass);

    CHECK(*pipeline != VK_NULL_HANDLE);
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"

#include "vulkan/vulkan.hpp"

#include <iterator>
#include <utility>
#include <vector>

constexpr int SH_COUNT = 16;
struct GaussianSplat {
    glm::vec3 centroid{ 0.0f };
    glm::vec3 scale{ 0.0f };
    glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
    float opacity{ 0.0f };
    std::array<glm::vec3, SH_COUNT> sphericalHarmonics{};
};

static const std::string SHADER_PATH = std::string(SPOCK_SOURCE_DIR) + "/samples/shaders/";
static const std::string VERTEX_SHADER = "splat.vs";
static const std::string FRAGMENT_SHADER = "splat.fs";

class SplatRenderer : public spock::Renderer
{
public:
    SplatRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0})
    {
        // Create the sample cube geometry and the uniform buffer for the model-view-projection matrix.
        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, *m_descriptorSetLayout}));

        const uint32_t SPLAT_VERTEX_COUNT = 1;

        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            SPLAT_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), SPLAT_VERTEX_DATA, SPLAT_VERTEX_COUNT);

        // Camera matrix.
        m_uniformBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            sizeof(glm::mat4x4),
            vk::BufferUsageFlagBits::eUniformBuffer);

        m_descriptorPool = spock::createDescriptorPool(m_device, {{vk::DescriptorType::eUniformBuffer, 1}});
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, {m_descriptorPool, *m_descriptorSetLayout}).front());
        spock::updateDescriptorSets(
            m_device,
            m_descriptorSet,
            {{vk::DescriptorType::eUniformBuffer,m_uniformBuffer.buffer(), VK_WHOLE_SIZE, nullptr}},
            {});

        createGraphicsPipeline();
    }

    void update(glm::mat4x4 const &viewProjClipMatrix)
    {
        spock::copyToDevice(m_uniformBuffer.deviceMemory(), viewProjClipMatrix);
    }

protected:
    void createGraphicsPipeline()
    {
        // Create the shaders.
        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShader{nullptr};
        vk::raii::ShaderModule fragmentShader{nullptr};
        try
        {
            vertexShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
            fragmentShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eFragment, SHADER_PATH + FRAGMENT_SHADER);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"},
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"},
        };

        std::vector<std::pair<vk::Format, uint32_t>> SPLAT_VERTEX_FORMAT{
            {vk::Format::eR32G32B32Sfloat, uint32_t(offsetof(GaussianSplat, centroid))},
            {vk::Format::eR32G32B32Sfloat, uint32_t(offsetof(GaussianSplat, scale))},
            {vk::Format::eR32G32B32A32Sfloat, uint32_t(offsetof(GaussianSplat, orientation))},
            {vk::Format::eR32Sfloat, uint32_t(offsetof(GaussianSplat, opacity))},
        };

        uint32_t offset = uint32_t(offsetof(GaussianSplat, sphericalHarmonics));

        for (uint32_t i = 0; i < SH_COUNT; ++i)
        {
            SPLAT_VERTEX_FORMAT.push_back({ vk::Format::eR32G32B32Sfloat, offset });
            offset += sizeof(glm::vec3);
        }

        // Finally create the graphics pipeline.
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            { m_device, vk::PipelineCacheCreateInfo() },
            shaderStagesInfo,
            sizeof(GaussianSplat),
            SPLAT_VERTEX_FORMAT,
            vk::PrimitiveTopology::ePointList,
            vk::FrontFace::eClockwise,
            true,
            m_pipelineLayout,
            m_renderPass);
    }

    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);

        // Draw all the scene, but for this example it's just a single cube.
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, {0});
        commandBuffer.draw(SPLAT_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_uniformBuffer;

};

class SplatApp : public spock::App
{
public:
    SplatApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Splat",
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
        return std::make_unique<SplatRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        SplatRenderer* renderer = static_cast<SplatRenderer*>(m_renderer.get());

        vk::Offset2D cursor = m_window.cursorPosition();
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            static const float sensitivity = 0.005f;
            m_camera.update(glm::vec2(
                static_cast<float>(m_previousCursor.x - cursor.x) * sensitivity,
                static_cast<float>(cursor.y - m_previousCursor.y) * sensitivity));
        }
        m_previousCursor = cursor;

        renderer->update(m_camera.viewProjClipMatrix(m_window.extents()));
    }

private:
    vk::Offset2D m_previousCursor{};
    spock::OrbitCamera m_camera{glm::vec3(0.0f), 5.0f};
};

int main()
{
    return spock::runApp<SplatApp>(500, 500);
}

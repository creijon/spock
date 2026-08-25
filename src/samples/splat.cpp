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
#include <vector>

struct SplatVertex
{
    glm::vec4 pos;
    glm::vec4 rgba;
};

static const SplatVertex SPLAT_VERTEX_DATA[] =
{
    // red face
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    // green face
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    // blue face
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    // yellow face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    // magenta face
    {{ 1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
    // cyan face
    {{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}
};

static constexpr uint32_t SPLAT_VERTEX_BUFFER_SIZE{sizeof(SPLAT_VERTEX_DATA)};
static constexpr uint32_t SPLAT_VERTEX_COUNT{std::size(SPLAT_VERTEX_DATA)};
static constexpr uint32_t SPLAT_VERTEX_STRIDE{sizeof(SPLAT_VERTEX_DATA[0])};
static const std::vector<std::pair<vk::Format, uint32_t>> SPLAT_VERTEX_FORMAT{
    {vk::Format::eR32G32B32A32Sfloat, 0},
    {vk::Format::eR32G32B32A32Sfloat, uint32_t(offsetof(SplatVertex, rgba))}
};

static const std::string SHADER_PATH = std::string(SPOCK_SOURCE_DIR) + "/samples/shaders/";
static const std::string VERTEX_SHADER = "splat.vs";
static const std::string FRAGMENT_SHADER = "splat.fs";

class SplatRenderer : public spock::Renderer
{
public:
    SplatRenderer(
        vk::raii::Instance const& instance,
        vk::SurfaceKHR const& windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            windowSurface,
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0})
    {
        // Create the sample cube geometry and the uniform buffer for the model-view-projection matrix.
        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, *m_descriptorSetLayout}));

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

    void setView(glm::vec3 const& view)
    {
        m_view = view;
    }

protected:
    void createGraphicsPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
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

        // Finally create the graphics pipeline.
        vk::raii::PipelineCache pipelineCache(m_device, vk::PipelineCacheCreateInfo());
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            pipelineCache,
            vertexShader, nullptr,
            fragmentShader, nullptr,
            SPLAT_VERTEX_STRIDE, SPLAT_VERTEX_FORMAT,
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

        // Update the push constants.
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, -1.0f, 0.0f);
        glm::mat4x4 mvpcMatrix = spock::viewProjClipMatrix(m_extents, m_view, target, up);
        spock::copyToDevice(m_uniformBuffer.deviceMemory(), mvpcMatrix);
        
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

    glm::vec3 m_view{};
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
        vk::SurfaceKHR const& windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<SplatRenderer>(instance, windowSurface, extents);
    }

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();
        
        SplatRenderer* renderer = static_cast<SplatRenderer*>(m_renderer.get());

        renderer->setView(glm::vec3(sinf(angle) * 5.0f, -3.0f, cosf(angle) * 5.0f));
    }
};

int main()
{
    return spock::runApp<SplatApp>(500, 500);
}

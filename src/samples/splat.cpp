// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "splat_loader.h"

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <execution>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

using QuadVertex = glm::vec2;

static constexpr QuadVertex quadCorners[]
{
    {-1.0f, -1.0f},
    {1.0f, -1.0f},
    {-1.0f, 1.0f},
    {1.0f, 1.0f},
};
static constexpr uint32_t QUAD_VERTEX_COUNT = std::size(quadCorners);

struct SortingEntry
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32Sfloat, 0 },
            { vk::Format::eR32Uint, offsetof(SortingEntry, index) }
        };
    }

    float zDist;
    uint32_t index;
};


struct PushConstants
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec2 viewport;
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
    }

    void update(SplatScene const &scene, spock::OrbitCamera const &camera, bool cameraMoved, vk::Extent2D const &viewExtents)
    {
        m_frameConstants.view = camera.view();
        m_frameConstants.proj = camera.projection(viewExtents);
        m_frameConstants.viewport = { viewExtents.width, viewExtents.height };

        if (cameraMoved)
        {
            for (uint32_t i = 0; i < m_splatCount; ++i)
            {
                glm::vec4 viewPos = m_frameConstants.view * glm::vec4(scene.instances[i].position, 1.0f);
                m_sorting[i].zDist = viewPos.z;
                m_sorting[i].index = i;
            }

            std::sort(
                std::execution::par,
                m_sorting.begin(), m_sorting.end(),
                [](const SortingEntry& a, const SortingEntry& b) { return a.zDist < b.zDist; });

            spock::copyToDevice(m_sortingBuffer.deviceMemory(), m_sorting.data(), m_splatCount);
        }
    }

    void createResources(SplatScene const &scene)
    {
        m_splatCount = uint32_t(scene.instances.size());

        spock::BindingData splatBinding{ 0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex };
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants) };

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            { splatBinding });
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, *m_descriptorSetLayout, pushConstantRange }));

        // Upload the splat data into a storage buffer.
        m_splatStorage = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            sizeof(SplatInstance) * m_splatCount,
            vk::BufferUsageFlagBits::eStorageBuffer);
        spock::copyToDevice(m_splatStorage.deviceMemory(), scene.instances.data(), m_splatCount);

        m_descriptorPool = spock::createDescriptorPool(
            m_device,
            { {vk::DescriptorType::eStorageBuffer, 1} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, { m_descriptorPool, *m_descriptorSetLayout }).front());
        spock::updateDescriptorSets(
            m_device,
            m_descriptorSet,
            {
                {vk::DescriptorType::eStorageBuffer, m_splatStorage.buffer(), VK_WHOLE_SIZE, nullptr}
            },
            {});

        // Create a small vertex buffer for the quad rendering.
        m_quadBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            QUAD_VERTEX_COUNT * sizeof(QuadVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_quadBuffer.deviceMemory(), quadCorners, QUAD_VERTEX_COUNT);

        // Create a indirection buffer, which will be used to sort the splats back-to-front.
        m_sorting.resize(m_splatCount);

        m_sortingBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            m_splatCount * sizeof(SortingEntry),
            vk::BufferUsageFlagBits::eVertexBuffer);

        createGraphicsPipeline();
    }

    void createGraphicsPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        // Create the shaders.
        glslang::InitializeProcess();
        try
        {
            if (shaderStages & vk::ShaderStageFlagBits::eVertex)
            {
                m_vertexShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
            }

            if (shaderStages & vk::ShaderStageFlagBits::eFragment)
            {
                m_fragmentShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eFragment, SHADER_PATH + FRAGMENT_SHADER);
            }
        }
        catch (std::exception const& e)
        {
            spock::writeLog("Error compiling shaders: %s\n" + std::string(e.what()));
        }
        glslang::FinalizeProcess();

        if (m_vertexShader != nullptr && m_fragmentShader != nullptr)
        {
            const vk::PipelineShaderStageCreateFlags shaderStageCreateFlags{};
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
                {shaderStageCreateFlags, vk::ShaderStageFlagBits::eVertex, *m_vertexShader, "main"},
                {shaderStageCreateFlags, vk::ShaderStageFlagBits::eFragment, *m_fragmentShader, "main"},
            };

            spock::VertexFormat vertexFormat;

            vertexFormat.addAttributes({ {vk::Format::eR32G32Sfloat, 0} }, sizeof(QuadVertex));
            vertexFormat.addAttributes<SortingEntry>(1, vk::VertexInputRate::eInstance);

            m_graphicsPipeline = spock::createGraphicsPipeline(
                m_device,
                shaderStagesInfo,
                m_pipelineLayout,
                m_renderPass,
                vertexFormat,
                vk::PrimitiveTopology::eTriangleStrip,
                vk::CullModeFlagBits::eNone,
                false);
            spock::writeLog("Shaders compiled successfully.\n");
        }
    }

protected:
    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // The graphics pipeline might be null if the shader compilation failed, so don't try to render in that case.
        if (m_graphicsPipeline == nullptr) return;

        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, m_frameConstants);
        commandBuffer.bindVertexBuffers(0, {m_quadBuffer.buffer(), m_sortingBuffer.buffer()}, {0, 0});

        commandBuffer.draw(QUAD_VERTEX_COUNT, m_splatCount, 0, 0);
    }

private:
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};
    vk::raii::ShaderModule m_vertexShader{ nullptr };
    vk::raii::ShaderModule m_fragmentShader{ nullptr };

    spock::BufferWrapper m_splatStorage;    // The splat data.
    spock::BufferWrapper m_sortingBuffer;   // The ordering of the splats for rendering.
    spock::BufferWrapper m_quadBuffer;      // The quad that is instanced.

    uint32_t m_splatCount = 0;
    glm::vec4 m_sceneBounds;
    std::vector<SortingEntry> m_sorting;
    PushConstants m_frameConstants{};
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
        auto renderer = std::make_unique<SplatRenderer>(instance, std::move(windowSurface), extents);

        loadScene(std::string(SPOCK_SOURCE_DIR) + "/samples/splats/tomatoes.ply");

        renderer->createResources(m_scene);

        m_camera.setFocus(m_sceneBounds);
        m_camera.setDistance(m_sceneBounds.w * 4.0f);

        return renderer;
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
            m_cameraMoved = true;

            m_previousCursor = cursor;
        }

        renderer->update(m_scene, m_camera, m_cameraMoved, m_window.extents());
        m_cameraMoved = false;
    }

private:
    void loadScene(const std::string& filename)
    {
        try
        {
            loadPly(filename, m_scene);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Failed to load splat PLY file: " + std::string(e.what()));
        }

        m_sceneBounds = m_scene.computeBounds();
    }

    SplatScene m_scene;
    glm::vec4 m_sceneBounds{};

    vk::Offset2D m_previousCursor{};
    spock::OrbitCamera m_camera{glm::vec3(0.0f), 5.0f};
    bool m_cameraMoved{true};
};

int main()
{
    return spock::runApp<SplatApp>(500, 500);
}

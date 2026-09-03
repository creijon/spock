// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "splat_loader.h"

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/file_watcher.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <efsw/efsw.hpp>

#include <iterator>
#include <utility>
#include <vector>

struct QuadVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return { {vk::Format::eR32G32Sfloat, 0} };
    }

    glm::vec2 pos;
};

static constexpr QuadVertex quadCorners[]
{
    {{-1.0f, -1.0f}},
    {{1.0f, -1.0f}},
    {{-1.0f, 1.0f}},
    {{1.0f, 1.0f}},
};
static constexpr uint32_t QUAD_VERTEX_COUNT = sizeof(quadCorners) / sizeof(QuadVertex);

struct CameraUniforms
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
        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, *m_descriptorSetLayout}));

        // Camera uniforms.
        m_uniformBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            sizeof(CameraUniforms),
            vk::BufferUsageFlagBits::eUniformBuffer);

        m_descriptorPool = spock::createDescriptorPool(m_device, {{vk::DescriptorType::eUniformBuffer, 1}});
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, {m_descriptorPool, *m_descriptorSetLayout}).front());
        spock::updateDescriptorSets(
            m_device,
            m_descriptorSet,
            {{vk::DescriptorType::eUniformBuffer, m_uniformBuffer.buffer(), VK_WHOLE_SIZE, nullptr}},
            {});
    }

    void update(spock::OrbitCamera const &camera, vk::Extent2D const &viewExtents)
    {
        CameraUniforms ubo
        {
            camera.view(),
            camera.projection(viewExtents),
            {viewExtents.width, viewExtents.height}
        };

        spock::copyToDevice(m_uniformBuffer.deviceMemory(), ubo);
    }

    void createResources(SplatScene const &scene)
    {
        // Create a small vertex buffer for the quad rendering.

        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            QUAD_VERTEX_COUNT * sizeof(QuadVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), quadCorners, QUAD_VERTEX_COUNT);

        // Upload the splat instances into the vertex buffer.
        m_splatCount = uint32_t(scene.instances.size());

        m_instanceBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            m_splatCount * sizeof(SplatInstance),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_instanceBuffer.deviceMemory(), scene.instances.data(), m_splatCount);

        // Upload the splat spherical harmonics into a uniform buffer.
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

            // The quad corners are the vertex data.
            vertexFormat.addAttributes<QuadVertex>();

            // The gaussian data is the per-instance data.
            vertexFormat.addAttributes<SplatInstance>(1, vk::VertexInputRate::eInstance);

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
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer(), m_instanceBuffer.buffer()}, {0, 0});

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

    spock::BufferWrapper m_uniformBuffer;
    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_instanceBuffer;

    uint32_t m_splatCount = 0;
    glm::vec4 m_sceneBounds;
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
        }

        if (m_watcher.modifiedShaders)
        {
            // If the shader source is changed then rebuild the shaders and recreate the graphics pipeline.
            renderer->waitIdle();
            renderer->createGraphicsPipeline(m_watcher.modifiedShaders);
            m_watcher.modifiedShaders = vk::ShaderStageFlags(0);
        }

        m_previousCursor = cursor;
        renderer->update(m_camera, m_window.extents());
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

    struct Watcher : public spock::FileWatcher
    {
        Watcher() : spock::FileWatcher(SHADER_PATH)
        {}

        void fileModified(std::string const& filename) override
        {
            if (filename == VERTEX_SHADER) modifiedShaders |= vk::ShaderStageFlagBits::eVertex;
            if (filename == FRAGMENT_SHADER) modifiedShaders |= vk::ShaderStageFlagBits::eFragment;
        }

        vk::ShaderStageFlags modifiedShaders{ vk::ShaderStageFlagBits::eAllGraphics };
    };

    Watcher m_watcher;
};

int main()
{
    return spock::runApp<SplatApp>(500, 500);
}

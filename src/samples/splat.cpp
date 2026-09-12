// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample demonstrates instanced rendering and the use of uniform and storage buffers.
// It implements a basic form of 3D Gaussian Splatting, derived from the tutorial:
// Feldman, Benjamin. (May 2026). “3D Gaussian Splatting in a Weekend”. bfeldman.me.
// https://bfeldman.me/3dgs-weekend/
// Some of the code is adapted from the original tutorial, but the rendering pipeline and resource
// management are implemented using Vulkan and the Spock framework.

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
#include <mutex>
#include <numeric>
#include <utility>
#include <vector>

// Required for Apple platforms to use parallel execution policy with std::sort.
#if defined(__APPLE__)
#include <oneapi/dpl/execution>
#include <oneapi/dpl/algorithm>
#endif

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


// Padded to match the std140 layout of the FrameConstants uniform block in splat.vs
struct FrameConstants
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
    glm::vec4 viewport;
};

static const std::string SHADER_PATH = std::string(SPOCK_DIR) + "/src/samples/shaders/";
static const std::string VERTEX_SHADER = "splat.vs";
static const std::string FRAGMENT_SHADER = "splat.fs";

static const std::string SPLAT_PATH = std::string(SPOCK_DIR) + "/assets/splats/tomatoes/scene.ply";

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
        PerFrameData& frameData = m_frameData[m_inFlightIndex];

        FrameConstants frameConstants{
            camera.view(),
            camera.projection(viewExtents),
            glm::vec4(camera.position(), 1.0f),
            glm::vec4(viewExtents.width, viewExtents.height, 0.0f, 0.0f)
        };

        memcpy(frameData.uniforms.map(), &frameConstants, sizeof(frameConstants));

        // We have to update the sorting data for all the frames in flight or if the camera moves.
        if (m_framesSinceResize < m_framesInFlight || cameraMoved)
        {
            // Rebuild the sorting data with the new Z distances.
            // Because we sort the vertex buffer it is important to do this on an array on the host
            // and then copy this over to the GPU in one memcpy.  This is because random reads from
            // the mapped memory are extremely slow, and the multithreading makes it even worse.
            for (uint32_t i = 0; i < m_splatCount; ++i)
            {
                glm::vec4 viewPos = frameConstants.view * glm::vec4(scene.instances[i].position, 1.0f);
                m_sorting[i].zDist = viewPos.z;
                m_sorting[i].index = i;
            }

            // Sort the splats, back to front.
            // Uses parallel execution policy to speed up sorting on large splat counts.
#if defined(__APPLE__)
            using namespace oneapi::dpl;
#else
            using namespace std;
#endif
            sort(execution::par, m_sorting.begin(), m_sorting.end(),
                [](const SortingEntry& a, const SortingEntry& b) { return a.zDist < b.zDist; });

            // Copy to the instanced vertex buffer.
            memcpy(frameData.sorting.map(), m_sorting.data(), m_splatCount * sizeof(SortingEntry));
        }
    }

    void createResources(SplatScene const& scene)
    {
        m_splatCount = uint32_t(scene.instances.size());

        spock::BindingData frameBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex };
        spock::BindingData splatBinding{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex };

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            { frameBinding, splatBinding });
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, *m_descriptorSetLayout }));

        // Upload the splat data into a storage buffer.
        m_splatStorage = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            sizeof(SplatInstance) * m_splatCount,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_splatStorage.upload(
            m_physicalDevice,
            m_device,
            m_commandPool,
            m_presenter->graphicsQueue(),
            scene.instances);

        // Create a small vertex buffer for the quad rendering.
        m_quadBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            QUAD_VERTEX_COUNT * sizeof(QuadVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_quadBuffer.deviceMemory(), quadCorners, QUAD_VERTEX_COUNT);
        

        m_descriptorPool = spock::createDescriptorPool(
            m_device,
            { {vk::DescriptorType::eUniformBuffer, m_framesInFlight},
              {vk::DescriptorType::eStorageBuffer, m_framesInFlight} });

        vk::MemoryPropertyFlags hostBacked{ vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

        for (uint32_t i = 0; i < m_framesInFlight; ++i)
        {
            m_frameData.push_back({
                std::move(vk::raii::DescriptorSets(m_device, { m_descriptorPool, *m_descriptorSetLayout }).front()),
                spock::BufferWrapper(
                    m_physicalDevice,
                    m_device,
                    sizeof(FrameConstants),
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    hostBacked),
                spock::BufferWrapper(
                    m_physicalDevice,
                    m_device,
                    m_splatCount * sizeof(SortingEntry),
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    hostBacked)
                });

            spock::updateDescriptorSets(
                m_device,
                m_frameData.back().descriptorSet,
                {
                    {vk::DescriptorType::eUniformBuffer, m_frameData.back().uniforms.buffer(), VK_WHOLE_SIZE, nullptr},
                    {vk::DescriptorType::eStorageBuffer, m_splatStorage.buffer(), VK_WHOLE_SIZE, nullptr}
                },
                {});
        }

        // Create an indirection buffer, which will be used to sort the splats back-to-front.
        // This is populated, sorted and uploaded in the Update() function when the camera moves.
        m_sorting.resize(m_splatCount);

        createPipeline();
    }

    void createPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShader{ nullptr };
        vk::raii::ShaderModule fragmentShader{ nullptr };
        try
        {
            if (shaderStages & vk::ShaderStageFlagBits::eVertex)
            {
                vertexShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
            }

            if (shaderStages & vk::ShaderStageFlagBits::eFragment)
            {
                fragmentShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eFragment, SHADER_PATH + FRAGMENT_SHADER);
            }
        }
        catch (std::exception const& e)
        {
            spock::writeLog(std::string(e.what()));
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        if (vertexShader != nullptr && fragmentShader != nullptr)
        {
            const vk::PipelineShaderStageCreateFlags shaderStageCreateFlags{};
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
                {shaderStageCreateFlags, vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"},
                {shaderStageCreateFlags, vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"},
            };

            spock::VertexFormat vertexFormat;
            vertexFormat.addAttributes({ {vk::Format::eR32G32Sfloat, 0} }, sizeof(QuadVertex));
            vertexFormat.addAttributes<SortingEntry>(1, vk::VertexInputRate::eInstance);

            m_graphicsPipeline = spock::createGraphicsPipeline(
                m_device,
                shaderStagesInfo,
                m_pipelineLayout,
                m_renderPass.renderPass(),
                vertexFormat,
                vk::PrimitiveTopology::eTriangleStrip,
                vk::CullModeFlagBits::eNone,
                false);
            spock::writeLog("Shaders compiled successfully.\n");
        }
    }

    bool initialising() const { return m_frameCount < m_framesInFlight; }

protected:
    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // The graphics pipeline might be null if the shader compilation failed, so don't try to render in that case.
        if (m_graphicsPipeline == nullptr) return;

        // Bind the pipeline and vertex buffers.
        PerFrameData& frameData = m_frameData[m_inFlightIndex];

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {frameData.descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_quadBuffer.buffer(), frameData.sorting.buffer() }, { 0, 0 });

        commandBuffer.draw(QUAD_VERTEX_COUNT, m_splatCount, 0, 0);
    }

private:
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    // Dynamic data
    struct PerFrameData
    {
        vk::raii::DescriptorSet descriptorSet;
        spock::BufferWrapper uniforms;      // Per-frame constants.
        spock::BufferWrapper sorting;       // The ordering of the splats for rendering.
    };
    std::vector<PerFrameData> m_frameData;

    // Constant data.
    spock::BufferWrapper m_splatStorage;    // The splat data.
    spock::BufferWrapper m_quadBuffer;      // The quad that is instanced.

    uint32_t m_splatCount{0};
    std::vector<SortingEntry> m_sorting;
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

        loadScene(SPLAT_PATH);

        renderer->createResources(m_scene);

        m_camera.setFocus(m_sceneBounds);
        m_camera.setDistanceRange(m_sceneBounds.w * 2.5f, m_sceneBounds.w * 5.0f);
        m_camera.setDistance(m_sceneBounds.w * 4.0f);

        return renderer;
    }

    void update() override
    {
        SplatRenderer* renderer = static_cast<SplatRenderer*>(m_renderer.get());
        vk::Offset2D cursor = m_window.cursorPosition();

        bool cameraMoved = false;

        if (m_window.scrollWheelOffsetY() != 0.0)
        {
            static const float wheelSensitivity = 0.1f;
            m_camera.setDistance(m_window.scrollWheelOffsetY() * wheelSensitivity + m_camera.distance());
            cameraMoved = true;
        }
        
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            static const float sensitivity = 0.005f;

            m_camera.update(glm::vec2(
                static_cast<float>(m_previousCursor.x - cursor.x) * sensitivity,
                static_cast<float>(cursor.y - m_previousCursor.y) * sensitivity));
            cameraMoved = true;
        }

        renderer->update(m_scene, m_camera, cameraMoved, m_window.extents());
        m_previousCursor = cursor;
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
    spock::OrbitCamera m_camera{glm::vec3(0.0f), 5.0f, 5.0f};
};

int main()
{
    return spock::runApp<SplatApp>(500, 500);
}

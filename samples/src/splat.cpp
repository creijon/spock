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
#include "spock/file_watcher.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <execution>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
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

static const std::string SHADER_PATH = std::string(SPOCK_DIR) + "/samples/assets/shaders/";
static const std::string VERTEX_SHADER = "splat.vs";
static const std::string FRAGMENT_SHADER = "splat.fs";

static const std::array<std::string, 4> SPLAT_FILES = {
    "amphimallon",
    "pachnoda",
    "tomatoes",
    "tetrarthria"
};

static const std::string SPLAT_PATH = std::string(SPOCK_DIR) + "/samples/assets/splats/";

// Splat scenes use the OpenCV convention (+Y down, +Z forward), rotate 180 degrees about X to get to our Y-up world.
static const glm::mat4 SPLAT_TO_WORLD = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

class SplatFrameState : public spock::FrameState
{
public:
    explicit SplatFrameState(spock::FoundryPtr const &foundry)
        : spock::FrameState(foundry->device(), foundry->commandPool())
    {
    }

    void createResources(
        spock::FoundryPtr const &foundry,
        vk::raii::DescriptorPool const &descriptorPool,
        vk::raii::DescriptorSetLayout const &descriptorSetLayout,
        spock::BufferWrapper const &splatStorage,
        uint32_t splatCount)
    {
        // The renderer creates frames on the first render after construction, resize or a scene load.
        // Allocate sample resources on first use, after the frame's fence wait.
        if (descriptorSet != nullptr) return;

        uniforms = spock::BufferWrapper(foundry, sizeof(FrameConstants), vk::BufferUsageFlagBits::eUniformBuffer);
        sorting = spock::BufferWrapper(foundry, splatCount * sizeof(SortingEntry), vk::BufferUsageFlagBits::eVertexBuffer);
        descriptorSet = std::move(vk::raii::DescriptorSets(foundry->device(), {descriptorPool, *descriptorSetLayout}).front());
        spock::updateDescriptorSets(foundry->device(), descriptorSet, {uniforms, splatStorage}, {});
    }

    void update(FrameConstants const &constants, std::vector<SortingEntry> const &entries, uint64_t revision)
    {
        memcpy(uniforms.map(), &constants, sizeof(constants));
        if (sortingRevision != revision)
        {
            memcpy(sorting.map(), entries.data(), entries.size() * sizeof(SortingEntry));
            sortingRevision = revision;
        }
    }

    spock::BufferWrapper uniforms;
    spock::BufferWrapper sorting;
    vk::raii::DescriptorSet descriptorSet{nullptr};
    uint64_t sortingRevision{0};
};

class SplatRenderer : public spock::Renderer
{
public:
    SplatRenderer(
        spock::FoundryPtr const &foundry,
        vk::Extent2D const& extents)
        : spock::Renderer(
            foundry,
            extents,
            {0.05f, 0.08f, 0.15f, 1.0f},
            {1.0f, 0},
            false)
    {
        m_createFrameFunc = [](spock::FoundryPtr const& foundry)
        {
            return std::make_unique<SplatFrameState>(foundry);
        };
    }

    ~SplatRenderer() override
    {
        waitIdle();
        // Release frame descriptors before destroying the storage buffer they reference.
        m_framePool.reset();
    }

    void update(SplatScene const &scene, spock::OrbitCamera const &camera, bool cameraMoved, vk::Extent2D const &viewExtents)
    {
        // App::update runs before frame acquisition, so only prepare host data here.
        m_frameConstants = FrameConstants{
            camera.view() * SPLAT_TO_WORLD,
            camera.projection(viewExtents),
            SPLAT_TO_WORLD * glm::vec4(camera.position(), 1.0f), // Camera position in splat space (the transform is its own inverse).
            glm::vec4(viewExtents.width, viewExtents.height, 0.0f, 0.0f)
        };

        if (m_sortingRevision == 0 || cameraMoved)
        {
            // Rebuild the sorting data with the new Z distances.
            // Because we sort the vertex buffer it is important to do this on an array on the host
            // and then copy this over to the GPU in one memcpy.  This is because random reads from
            // the mapped memory are extremely slow, and the multithreading makes it even worse.
            for (uint32_t i = 0; i < m_splatCount; ++i)
            {
                glm::vec4 viewPos = m_frameConstants.view * glm::vec4(toVec3(scene.instances[i].position), 1.0f);
                m_sorting[i].zDist = viewPos.z;
                m_sorting[i].index = i;
            }

            // Sort the splats, back to front.  All view-space Z values are negative, so lowest is furthest away.
            // Uses parallel execution policy to speed up sorting on large splat counts.
#if defined(__APPLE__)
            using namespace oneapi::dpl;
#else
            using namespace std;
#endif
            sort(execution::par, m_sorting.begin(), m_sorting.end(),
                [](const SortingEntry& a, const SortingEntry& b) { return a.zDist < b.zDist; });

            // Each frame uploads this ordering when it is next acquired, even after the camera stops moving.
            ++m_sortingRevision;
        }
    }

    void createResources(SplatScene const& scene)
    {
        waitIdle();
        // Drop frames holding resources from the previous scene; renderFrame recreates them with m_createFrameFunc.
        m_framePool.reset();
        m_splatCount = uint32_t(scene.instances.size());

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_foundry->device(),
            vk::ShaderStageFlagBits::eVertex,
            {vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer});
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_foundry->device(), { {}, *m_descriptorSetLayout }));

        m_descriptorPool = spock::createDescriptorPool(
            m_foundry->device(),
            {{vk::DescriptorType::eUniformBuffer, m_framesInFlight},
             {vk::DescriptorType::eStorageBuffer, m_framesInFlight}});

        // Upload the splat data into a storage buffer.
        m_splatStorage = spock::BufferWrapper(
            m_foundry,
            sizeof(SplatInstance) * m_splatCount,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_splatStorage.upload(m_foundry, scene.instances);

        // Create a small vertex buffer for the quad rendering.
        m_quadBuffer = spock::BufferWrapper(
            m_foundry,
            QUAD_VERTEX_COUNT * sizeof(QuadVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_quadBuffer.deviceMemory(), quadCorners, QUAD_VERTEX_COUNT);

        // Create an indirection buffer, which will be used to sort the splats back-to-front.
        // Update prepares the ordering on the host; each acquired frame uploads its own copy.
        m_sorting.resize(m_splatCount);
        m_sortingRevision = 0;

        createPipeline();
    }

    void createPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        try
        {
            if ((m_vertShader == nullptr) || (shaderStages & vk::ShaderStageFlagBits::eVertex))
            {
                m_vertShader = spock::loadShader(m_foundry->device(), vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
            }

            if ((m_fragShader == nullptr) || (shaderStages & vk::ShaderStageFlagBits::eFragment))
            {
                m_fragShader = spock::loadShader(m_foundry->device(), vk::ShaderStageFlagBits::eFragment, SHADER_PATH + FRAGMENT_SHADER);
            }
        }
        catch (std::exception const& e)
        {
            spock::writeLog(std::string(e.what()));
        }
        if (m_vertShader != nullptr && m_fragShader != nullptr)
        {
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
                {{}, vk::ShaderStageFlagBits::eVertex, *m_vertShader, "main"},
                {{}, vk::ShaderStageFlagBits::eFragment, *m_fragShader, "main"},
            };

            spock::VertexFormat vertexFormat;
            vertexFormat.addAttributes({ {vk::Format::eR32G32Sfloat, 0} }, sizeof(QuadVertex));
            vertexFormat.addAttributes<SortingEntry>(1, vk::VertexInputRate::eInstance);

            m_graphicsPipeline = spock::createGraphicsPipeline(
                m_foundry->device(),
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
    void render(spock::FrameState &frame) override
    {
        // The graphics pipeline might be null if the shader compilation failed, so don't try to render in that case.
        if (m_graphicsPipeline == nullptr) return;

        auto& frameData = static_cast<SplatFrameState&>(frame);
        frameData.createResources(m_foundry, m_descriptorPool, m_descriptorSetLayout, m_splatStorage, m_splatCount);
        frameData.update(m_frameConstants, m_sorting, m_sortingRevision);

        auto& commandBuffer = frameData.commandBuffer;

        // Bind the pipeline and vertex buffers.

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {frameData.descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_quadBuffer.buffer(), frameData.sorting.buffer() }, { 0, 0 });

        commandBuffer.draw(QUAD_VERTEX_COUNT, m_splatCount, 0, 0);
    }

private:
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::ShaderModule m_vertShader{ nullptr };
    vk::raii::ShaderModule m_fragShader{ nullptr };

    // Constant data.
    spock::BufferWrapper m_splatStorage;    // The splat data.
    spock::BufferWrapper m_quadBuffer;      // The quad that is instanced.

    uint32_t m_splatCount{0};
    FrameConstants m_frameConstants{};
    std::vector<SortingEntry> m_sorting;
    uint64_t m_sortingRevision{0};
};

class SplatApp : public spock::App
{
public:
    SplatApp(uint32_t windowWidth, uint32_t windowHeight, uint32_t sceneIndex)
        : spock::App(
            "Splat",
            windowWidth,
            windowHeight)
        , m_sceneIndex(sceneIndex)
        , m_watcher(SHADER_PATH)
    {
    }

protected:
    spock::FoundryPtr createFoundry() const override
    {
        std::vector<char const*> extensions{
            VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
            VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME
        };

        vk::PhysicalDeviceShaderFloat16Int8Features float16Int8{
            VK_TRUE,
            VK_FALSE,
            nullptr
        };

        vk::PhysicalDevice16BitStorageFeatures storage16{
            VK_TRUE,
            VK_TRUE,
            VK_FALSE,
            VK_FALSE,
            &float16Int8
        };

        return std::make_shared<const spock::Foundry>(
            m_instance,
            m_window.createSurface(m_instance),
            vk::QueueFlagBits::eGraphics,
            vk::PhysicalDeviceType::eDiscreteGpu,
            extensions,
            &storage16);
    }

    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        auto renderer = std::make_unique<SplatRenderer>(m_foundry, m_window.extents());

        loadScene();
        renderer->createResources(m_scene);

        m_camera.setFocus(glm::vec3(SPLAT_TO_WORLD * glm::vec4(glm::vec3(m_sceneBounds), 1.0f)));
        m_camera.setDistanceRange(m_sceneBounds.w * 1.5f, m_sceneBounds.w * 5.0f);
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

        // Check the file watcher for any modified shader files and rebuild the pipeline if necessary.
        auto modifiedFiles = m_watcher.takeModified();
        vk::ShaderStageFlags modifiedShaders{0};
        if (modifiedFiles.contains(VERTEX_SHADER)) modifiedShaders |= vk::ShaderStageFlagBits::eVertex;
        if (modifiedFiles.contains(FRAGMENT_SHADER)) modifiedShaders |= vk::ShaderStageFlagBits::eFragment;

        if (modifiedShaders)
        {
            // If the shader source is changed then rebuild the shaders and recreate the graphics pipeline.
            renderer->waitIdle();
            renderer->createPipeline(modifiedShaders);
        }
    }

private:
    void loadScene()
    {
        std::string filename = SPLAT_PATH + SPLAT_FILES[m_sceneIndex] + "/scene.ply";

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
    uint32_t m_sceneIndex{ 1 };

    vk::Offset2D m_previousCursor{};
    spock::OrbitCamera m_camera{glm::vec3(0.0f), 5.0f, 5.0f, 45.0f, 0.01f, 100.0f};

    spock::FileWatcher m_watcher;
};

int main(int argc, char** argv)
{
    uint32_t sceneIndex = 2;

    if (argc > 1)
    {
        try
        {
            size_t parsedChars = 0;
            int parsed = std::stoi(argv[1], &parsedChars);
            if (parsedChars != std::string(argv[1]).size())
            {
                throw std::out_of_range("splat index out of range");
            }
            parsed = std::clamp(parsed, 0, static_cast<int>(SPLAT_FILES.size() - 1));
            sceneIndex = static_cast<uint32_t>(parsed);
        }
        catch (std::exception const&)
        {
            std::cerr << "Usage: splat [index]\n";
            std::cerr << "  index: an integer selecting which splat scene to load:\n";
            for (size_t i = 0; i < SPLAT_FILES.size(); ++i)
            {
                std::cerr << "    " << i << ": " << SPLAT_FILES[i] << "\n";
            }
            return -1;
        }
    }

    return spock::runApp<SplatApp>(500, 500, sceneIndex);
}

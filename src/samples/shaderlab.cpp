// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/file_watcher.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <functional>
#include <iterator>
#include <utility>
#include <vector>

struct ShaderLabVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {{ vk::Format::eR32G32Sfloat, 0 }};
    }

    glm::vec2 pos;
};

static const ShaderLabVertex SHADERLAB_VERTEX_DATA[] =
{
    {{-1.0f, -1.0f}},
    {{ 3.0f, -1.0f}},
    {{-1.0f,  3.0f}},
};

static const uint32_t SHADERLAB_VERTEX_BUFFER_SIZE{sizeof(SHADERLAB_VERTEX_DATA)};
static const uint32_t SHADERLAB_VERTEX_COUNT{std::size(SHADERLAB_VERTEX_DATA)};

static const std::string SHADER_PATH = std::string(SPOCK_DIR) + "/src/samples/shaders/shaderlab/";
static const std::string VERTEX_SHADER = "default.vs";
static const std::string FRAGMENT_SHADER = "default.fs";

// PushConstants have been defined to be mostly compatible with the ShaderToy interface.
// The full set is too big for the 42 byte push constant limit, so it'll have to be a uniform buffer.
struct PushConstants
{
    glm::vec4 iMouse;       // image/buffer xy = current pixel coords (if LMB is down). zw = click pixel
    glm::vec3 iResolution;  // image/buffer The viewport resolution (z is pixel aspect ratio, usually 1.0)
    float iTime;            // image/sound/buffer Current time in seconds
    int iFrame;             // image/buffer Current frame
};

class ShaderLabRenderer : public spock::Renderer
{
public:
    ShaderLabRenderer(
        vk::raii::Instance const &instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const &extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0})
    {
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eAllGraphics,
            0,
            sizeof(PushConstants) };

        m_pipelineLayout = vk::raii::PipelineLayout(m_device, { {}, {}, pushConstantRange });

        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            SHADERLAB_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(
            m_vertexBuffer.deviceMemory(),
            SHADERLAB_VERTEX_DATA,
            SHADERLAB_VERTEX_COUNT);
    }

    void createGraphicsPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
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

            m_graphicsPipeline =
                spock::createGraphicsPipeline(
                    m_device,
                    shaderStagesInfo,
                    m_pipelineLayout,
                    m_renderPass,
                    spock::VertexFormatWrapper<ShaderLabVertex>(),
                    vk::PrimitiveTopology::eTriangleList,
                    vk::CullModeFlagBits::eNone,
                    false);
            spock::writeLog("Shaders compiled successfully.\n");
        }
    }

    void setMousePos(vk::Offset2D const &mousePos)
    {
        m_mousePos = mousePos;
    }

    void setMouseClickPos(vk::Offset2D const& mouseClickPos)
    {
        m_mouseClickPos = mouseClickPos;
    }

protected:
    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // The graphics pipeline might be null if the shader compilation failed, so don't try to render in that case.
        if (m_graphicsPipeline == nullptr) return;

        // Bind the pipeline.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

        // Update the push constants.
        using Seconds = std::chrono::duration<float>;
        PushConstants pushConstants{
            glm::vec4((float)m_mousePos.x, (float)m_mousePos.y, (float)m_mouseClickPos.x, (float)m_mouseClickPos.y),
            glm::vec3((float)m_extents.width, (float)m_extents.height, 1.0f),
            std::chrono::duration_cast<Seconds>(time).count(),
            (int)m_frameCount};
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, pushConstants);

        // Draw the single triangle.
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, { 0 });
        commandBuffer.draw(SHADERLAB_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};
    vk::raii::ShaderModule m_vertexShader{nullptr};
    vk::raii::ShaderModule m_fragmentShader{nullptr};

    spock::BufferWrapper m_vertexBuffer;

    vk::Offset2D m_mousePos{0, 0};
    vk::Offset2D m_mouseClickPos{0, 0};
};

class ShaderLabApp : public spock::App
{
public:
    ShaderLabApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App("ShaderLab", windowWidth, windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<ShaderLabRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        ShaderLabRenderer* renderer = static_cast<ShaderLabRenderer*>(m_renderer.get());

        // Store the mouse position and click position for use in the next frame.
        vk::Offset2D mousePos{0, 0};

        mousePos = m_window.cursorPosition();
        renderer->setMousePos(mousePos);
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            renderer->setMouseClickPos(mousePos);
        }

        if (m_watcher.modifiedShaders)
        {
            // If the shader source is changed then rebuild the shaders and recreate the graphics pipeline.
            renderer->waitIdle();
            renderer->createGraphicsPipeline(m_watcher.modifiedShaders);
            m_watcher.modifiedShaders = vk::ShaderStageFlags(0);
        }
    }

private:
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
    return spock::runApp<ShaderLabApp>(500, 500);
}

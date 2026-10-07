// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample demonstrates automatic shader compilation and hot-reloading.
// As an example, the default shaders implement a simple ShaderToy-like interface, where the
// fragment shader can be experimented with in real-time.

#include "spock/app.hpp"
#include "spock/creators.hpp"
#include "spock/file_watcher.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
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
// Note: the order of these members is critical to maintain packing on the GLSL side.
// iMouse and iResolution are aligned to 16 bytes
// iTime and iFrame have 4-byte alignment on GLSL. 
struct PushConstants
{
    glm::vec4 iMouse;       // image/buffer xy = current pixel coords (if LMB is down). zw = click pixel
    glm::vec3 iResolution;  // image/buffer The viewport resolution (z is pixel aspect ratio, usually 1.0)
    float iTime;            // image/sound/buffer Current time in seconds
    int iFrame;             // image/buffer Current frame
}; // 34 bytes

class ShaderLabRenderer : public spock::Renderer
{
public:
    ShaderLabRenderer(
        std::shared_ptr<const spock::Foundry> const &foundry,
        vk::Extent2D const &extents)
        : spock::Renderer(
            foundry,
            extents,
            { 0.2f, 0.2f, 0.3f, 1.0 },
            { 1.0f, 0 })
    {
        createResources();
    }

    void createResources()
    {
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eAllGraphics,
            0,
            sizeof(PushConstants) };

        m_pipelineLayout = vk::raii::PipelineLayout(m_foundry->device(), {{}, {}, pushConstantRange});

        m_vertexBuffer = spock::BufferWrapper(
            m_foundry,
            SHADERLAB_VERTEX_BUFFER_SIZE,
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(
            m_vertexBuffer.deviceMemory(),
            SHADERLAB_VERTEX_DATA,
            SHADERLAB_VERTEX_COUNT);
    }

    void createPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        try
        {
            if (shaderStages & vk::ShaderStageFlagBits::eVertex)
            {
                m_vertShader = spock::loadShader(m_foundry->device(), vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
            }

            if (shaderStages & vk::ShaderStageFlagBits::eFragment)
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

            m_graphicsPipeline =
                spock::createGraphicsPipeline(
                    m_foundry->device(),
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

	void setTime(std::chrono::microseconds time)
	{
		m_time = std::chrono::duration_cast<std::chrono::seconds>(time);
	}

    void setMousePos(vk::Offset2D const &mousePos)
    {
        m_mousePos.x = mousePos.x;
        // Flip Y to make it consistent with OpenGL
        m_mousePos.y = m_extents.height - mousePos.y;
    }

    void setMouseClickPos(vk::Offset2D const& mouseClickPos)
    {
        m_mouseClickPos.x = mouseClickPos.x;
        // Flip Y to make it consistent with OpenGL
        m_mouseClickPos.y = m_extents.height - mouseClickPos.y;
    }

protected:
    void render(std::shared_ptr<spock::FrameState> const &frame) override
    {
        // The graphics pipeline might be null if the shader compilation failed, so don't try to render in that case.
        if (m_graphicsPipeline == nullptr) return;

        auto& commandBuffer = frame->commandBuffer;

        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });

        // Update the push constants.
        using Seconds = std::chrono::duration<float>;
        PushConstants pushConstants{
            glm::vec4((float)m_mousePos.x, (float)m_mousePos.y, (float)m_mouseClickPos.x, (float)m_mouseClickPos.y),
            glm::vec3((float)m_extents.width, (float)m_extents.height, 1.0f),
            std::chrono::duration_cast<Seconds>(m_time).count(),
            (int)m_frameCount};
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eAllGraphics, pushConstants);

        // Draw the single triangle.
        commandBuffer.draw(SHADERLAB_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};
    vk::raii::ShaderModule m_vertShader{nullptr};
    vk::raii::ShaderModule m_fragShader{nullptr};

    spock::BufferWrapper m_vertexBuffer;

    std::chrono::seconds m_time{0};
    vk::Offset2D m_mousePos{0, 0};
    vk::Offset2D m_mouseClickPos{0, 0};;
};

class ShaderLabApp : public spock::App
{
public:
    ShaderLabApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App("ShaderLab", windowWidth, windowHeight)
        , m_watcher(SHADER_PATH)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        return std::make_unique<ShaderLabRenderer>(m_foundry, m_window.extents());
    }

    void update() override
    {
        ShaderLabRenderer* renderer = static_cast<ShaderLabRenderer*>(m_renderer.get());
		renderer->setTime(m_time);

        // Store the mouse position and click position for use in the next frame.
        vk::Offset2D mousePos{0, 0};

        mousePos = m_window.cursorPosition();
        renderer->setMousePos(mousePos);
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            renderer->setMouseClickPos(mousePos);
        }

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
    spock::FileWatcher m_watcher;
};

int main()
{
    return spock::runApp<ShaderLabApp>(500, 500);
}

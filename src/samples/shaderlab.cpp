// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/creators.hpp"
#include "spock/framework.hpp"
#include "spock/math.hpp"
#include "spock/shaders.hpp"

#include "vulkan/vulkan.hpp"

#include <efsw/efsw.hpp>

#include <iostream>
#include <iterator>
#include <vector>

struct ShaderLabVertex
{
    glm::vec4 pos;
    glm::vec2 uv;
};

static const ShaderLabVertex SHADERLAB_VERTEX_DATA[] =
{
    {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 3.0f, -1.0f, 1.0f, 1.0f}, {2.0f, 0.0f}},
    {{-1.0f,  3.0f, 1.0f, 1.0f}, {0.0f, 2.0f}},
};

static const uint32_t SHADERLAB_VERTEX_BUFFER_SIZE{sizeof(SHADERLAB_VERTEX_DATA)};
static const uint32_t SHADERLAB_VERTEX_COUNT{std::size(SHADERLAB_VERTEX_DATA)};
static const uint32_t SHADERLAB_VERTEX_STRIDE{sizeof(SHADERLAB_VERTEX_DATA[0])};
static const std::vector<std::pair<vk::Format, uint32_t>> SHADERLAB_VERTEX_FORMAT{
    {vk::Format::eR32G32B32A32Sfloat, 0},
    {vk::Format::eR32G32Sfloat, uint32_t(offsetof(ShaderLabVertex, uv))}
};

static const std::string SHADER_PATH = std::string(SPOCK_SOURCE_DIR) + "/shaders/shaderlab/";
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

class ShaderLabApp : public spock::Framework
{
public:
    ShaderLabApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::Framework(
            "ShaderLab",
            windowWidth,
            windowHeight,
            {0.2f, 0.2f, 0.3f, 1.0f},
            {1.0f, 0},
            false)
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

        createGraphicsPipeline();

        m_fileWatcher = std::make_unique<efsw::FileWatcher>();
        m_listener = std::make_unique<UpdateListener>(*this);

        m_watchID = m_fileWatcher->addWatch(SHADER_PATH, m_listener.get());
        m_fileWatcher->watch();
    }

    ~ShaderLabApp()
    {
        m_fileWatcher->removeWatch(m_watchID);
    }

protected:
    void update() override
    {
        // Store the mouse position and click position for use in the next frame.
        glfwGetCursorPos(m_handle, &m_mousePos.x, &m_mousePos.y);
        if (glfwGetMouseButton(m_handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwGetCursorPos(m_handle, &m_mouseClickPos.x, &m_mouseClickPos.y);
        }

        if (m_modifiedShaders)
        {
            // If the shader source is changed then rebuild the shaders and recreate the graphics pipeline.
            m_device.waitIdle();
            createGraphicsPipeline(m_modifiedShaders);
            m_modifiedShaders = vk::ShaderStageFlags(0);
        }
    }

    void render(vk::raii::CommandBuffer const &commandBuffer) override
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
            std::chrono::duration_cast<Seconds>(m_time).count(),
            (int)m_frameCount};

        vk::ArrayProxyNoTemporaries<const uint8_t> dataSpan{
            sizeof(PushConstants),
            reinterpret_cast<const uint8_t*>(&pushConstants)};

        commandBuffer.pushConstants<uint8_t>(
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eAllGraphics,
            0,
            dataSpan);

        // Draw the single triangle.
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, { 0 });
        commandBuffer.draw(SHADERLAB_VERTEX_COUNT, 1, 0, 0);
    }

private:

    void shaderModified(std::string const& filename)
    {
        if (filename == VERTEX_SHADER) m_modifiedShaders |= vk::ShaderStageFlagBits::eVertex;
        if (filename == FRAGMENT_SHADER) m_modifiedShaders |= vk::ShaderStageFlagBits::eFragment;
    }

    void createGraphicsPipeline(vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAllGraphics)
    {
        glslang::InitializeProcess();

        if (shaderStages & vk::ShaderStageFlagBits::eVertex)
        {
            m_vertexShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eVertex, SHADER_PATH + VERTEX_SHADER);
        }

        if (shaderStages & vk::ShaderStageFlagBits::eFragment)
        {
            m_fragmentShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eFragment, SHADER_PATH + FRAGMENT_SHADER);
        }

        glslang::FinalizeProcess();

        if (m_vertexShader != nullptr && m_fragmentShader != nullptr)
        {
            m_graphicsPipeline =
                spock::createGraphicsPipeline(
                    m_device,
                    {m_device, vk::PipelineCacheCreateInfo()},
                    m_vertexShader, nullptr,
                    m_fragmentShader, nullptr,
                    SHADERLAB_VERTEX_STRIDE, SHADERLAB_VERTEX_FORMAT,
                    vk::FrontFace::eClockwise,
                    false,
                    m_pipelineLayout,
                    m_renderPass);
            spock::writeLog("Shaders compiled successfully.\n");
        }
    }

    class UpdateListener : public efsw::FileWatchListener
    {
    public:
        UpdateListener(ShaderLabApp& app)
            : m_app(app)
        {}

        void handleFileAction(
            efsw::WatchID watchid,
            const std::string& dir,
            const std::string& filename,
            efsw::Action action,
            const std::string& oldFilename) override
        {
            if (action == efsw::Actions::Modified)
            {
                m_app.shaderModified(filename);
            }
        }

    private:
        ShaderLabApp& m_app;
    };

    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};
    vk::raii::ShaderModule m_vertexShader{nullptr};
    vk::raii::ShaderModule m_fragmentShader{nullptr};

    spock::BufferWrapper m_vertexBuffer;

    std::unique_ptr<efsw::FileWatcher> m_fileWatcher;
    std::unique_ptr<UpdateListener> m_listener;
    efsw::WatchID m_watchID;
    vk::ShaderStageFlags m_modifiedShaders{0};

    glm::dvec2 m_mousePos{0.0, 0.0};
    glm::dvec2 m_mouseClickPos{0.0, 0.0};
};

int main()
{
    try
    {
        auto app = ShaderLabApp(500, 500);

        app.run();
    }
    catch (vk::SystemError &err)
    {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit(-1);
    }
    catch (std::exception &err)
    {
        std::cout << "std::exception: " << err.what() << std::endl;
        exit(-1);
    }
    catch (...)
    {
        std::cout << "unknown error\n";
        exit(-1);
    }
    return 0;
}

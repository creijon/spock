// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/creators.hpp"
#include "spock/framework.hpp"
#include "spock/math.hpp"
#include "spock/shaders.hpp"

#include "vulkan/vulkan.hpp"

#include <iostream>
#include <iterator>
#include <vector>

static const std::string SHADER_PATH = std::string(SPOCK_SOURCE_DIR) + "/shaders/";
static const std::string VERTEX_SHADER = "model.vs";
static const std::string FRAGMENT_SHADER = "model.fs";

struct PushConstants
{
    glm::mat4x4 mvp;
};

class ModelApp : public spock::Framework
{
public:
    ModelApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::Framework(
            "Model",
            windowWidth,
            windowHeight, 
            {0.2f, 0.2f, 0.3f, 1.0f}, 
            {1.0f, 0},
            true)
    {
        // Create the sample cube geometry and the push constants for the model-view-projection matrix.
        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, {}, pushConstantRange }));

        m_vertexBuffer = spock::BufferWrapper(m_physicalDevice, m_device, MODEL_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), MODEL_VERTEX_DATA, MODEL_VERTEX_COUNT);

        createGraphicsPipeline();

        m_fileWatcher = std::make_unique<efsw::FileWatcher>();
        m_listener = std::make_unique<UpdateListener>(*this);

        m_watchID = m_fileWatcher->addWatch(SHADER_PATH, m_listener.get());
        m_fileWatcher->watch();
    }

    ~ModelApp()
    {
        m_fileWatcher->removeWatch(m_watchID);
    }

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

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();
        m_view = glm::vec3(sinf(angle) * 5.0f, -3.0f, cosf(angle) * 5.0f);

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
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, -1.0f, 0.0f);
        PushConstants pushConstants{
            spock::createModelViewProjectionClipMatrix(m_extents, m_view, target, up)};

        vk::ArrayProxyNoTemporaries<const uint8_t> dataSpan{
            sizeof(PushConstants),
            reinterpret_cast<const uint8_t*>(&pushConstants)};

        commandBuffer.pushConstants<uint8_t>(
            m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            dataSpan);
        
        // Draw all the scene, but for this example it's just a single cube.
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, {0});
        commandBuffer.draw(MODEL_VERTEX_COUNT, 1, 0, 0);
    }

private:
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

    spock::BufferWrapper m_vertexBuffer;

    std::unique_ptr<efsw::FileWatcher> m_fileWatcher;
    std::unique_ptr<UpdateListener> m_listener;
    efsw::WatchID m_watchID;
    vk::ShaderStageFlags m_modifiedShaders{0};

    glm::vec3 m_view{};
};

int main()
{
    try
    {
        auto app = ModelApp(500, 500);

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

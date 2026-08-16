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
        spock::copyToDevice(m_vertexBuffer.deviceMemory, MODEL_VERTEX_DATA, MODEL_VERTEX_COUNT);

        // Create the shaders.
        glslang::InitializeProcess();
        auto vertexShaderModule = spock::compileShader(m_device, vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
        auto fragmentShaderModule = spock::compileShader(m_device, vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);
        glslang::FinalizeProcess();

        // Finally create the graphics pipeline.
        vk::raii::PipelineCache pipelineCache(m_device, vk::PipelineCacheCreateInfo());
        m_graphicsPipeline =
            spock::createGraphicsPipeline(m_device,
                                          pipelineCache,
                                          vertexShaderModule, nullptr,
                                          fragmentShaderModule, nullptr,
                                          MODEL_VERTEX_STRIDE, MODEL_VERTEX_FORMAT,
                                          vk::FrontFace::eClockwise,
                                          true,
                                          m_pipelineLayout,
                                          m_renderPass);
    }

    void update() override
    {
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();
        m_view = glm::vec3(sinf(angle) * 5.0f, -3.0f, cosf(angle) * 5.0f);
    }

    void render(vk::raii::CommandBuffer const &commandBuffer) override
    {
        // Bind the pipeline and vertex buffers.
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
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer}, {0});
        commandBuffer.draw(MODEL_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;

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

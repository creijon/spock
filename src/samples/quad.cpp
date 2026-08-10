#include "vulkan/vulkan.hpp"
#if defined(_MSC_VER)
// no need to ignore any warnings with MSVC
#elif defined(__GNUC__)
#if (9 <= __GNUC__)
#pragma GCC diagnostic ignored "-Winit-list-lifetime"
#endif
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include "spock/creators.hpp"
#include "spock/framework.hpp"
#include "spock/math.hpp"
#include "spock/shaders.hpp"

#include <iostream>
#include <iterator>
#include <vector>

static char const *AppName = "Quad";

struct QuadVertex
{
    float x, y, z, w; // Position
    float r, g, b, a; // Color
};

static const QuadVertex QUAD_VERTEX_DATA[] =
{
    // red face
    {-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}
};

static constexpr uint32_t QUAD_VERTEX_BUFFER_SIZE{sizeof(QUAD_VERTEX_DATA)};
static constexpr uint32_t QUAD_VERTEX_COUNT{std::size(QUAD_VERTEX_DATA)};
static constexpr uint32_t QUAD_VERTEX_STRIDE{sizeof(QUAD_VERTEX_DATA[0])};
static const std::vector<std::pair<vk::Format, uint32_t>> QUAD_VERTEX_FORMAT{
    {vk::Format::eR32G32B32A32Sfloat, 0},
    {vk::Format::eR32G32B32A32Sfloat, 16}
};


static const std::string VERTEX_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform buffer
{
  mat4 mvp;
} uniformBuffer;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = inColor;
  gl_Position = uniformBuffer.mvp * pos;
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 color;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = color;
}
)";

class QuadApp : public spock::Framework
{
public:
    QuadApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::Framework(
            AppName,
            windowWidth,
            windowHeight, 
            {0.2f, 0.2f, 0.3f, 1.0f}, 
            {1.0f, 0})
    {
        // Create the sample cube geometry and the uniform buffer for the model-view-projection matrix.
        m_descriptorSetLayout = spock::createDescriptorSetLayout(m_device, {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, *m_descriptorSetLayout}));

        m_vertexBuffer = spock::BufferWrapper(m_physicalDevice, m_device, QUAD_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory, QUAD_VERTEX_DATA, QUAD_VERTEX_COUNT);

        // Camera matrix.
        m_uniformBuffer = spock::BufferWrapper(m_physicalDevice, m_device, sizeof(glm::mat4x4), vk::BufferUsageFlagBits::eUniformBuffer);

        m_descriptorPool = spock::createDescriptorPool(m_device, {{vk::DescriptorType::eUniformBuffer, 1}});
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, {m_descriptorPool, *m_descriptorSetLayout}).front());
        spock::updateDescriptorSets(
            m_device, m_descriptorSet, {{vk::DescriptorType::eUniformBuffer, m_uniformBuffer.buffer, VK_WHOLE_SIZE, nullptr}}, {});

        // Create the shaders.
        glslang::InitializeProcess();
        auto vertexShaderModule = spock::makeShaderModule(m_device, vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
        auto fragmentShaderModule = spock::makeShaderModule(m_device, vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);
        glslang::FinalizeProcess();

        // Finally create the graphics pipeline.
        vk::raii::PipelineCache pipelineCache(m_device, vk::PipelineCacheCreateInfo());
        m_graphicsPipeline =
            spock::createGraphicsPipeline(m_device,
                                          pipelineCache,
                                          vertexShaderModule,
                                          nullptr,
                                          fragmentShaderModule,
                                          nullptr,
                                          QUAD_VERTEX_STRIDE,
                                          QUAD_VERTEX_FORMAT,
                                          vk::FrontFace::eClockwise,
                                          true,
                                          m_pipelineLayout,
                                          m_renderPass);
    }

    void update(std::chrono::microseconds frameTime) override
    {
        double angle = frameTime.count() * 0.000002;
        m_view = glm::vec3(sinf(angle) * 5.0f, -3.0f, cosf(angle) * 5.0f);
    }

    void render(vk::raii::CommandBuffer const &commandBuffer) override
    {
        // Bind the pipeline and vertex buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, {m_vertexBuffer.buffer}, {0});

        // Set the camera location.
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 up(0.0f, -1.0f, 0.0f);
        glm::mat4x4 mvpcMatrix = spock::createModelViewProjectionClipMatrix(m_extents, m_view, target, up);
        spock::copyToDevice(m_uniformBuffer.deviceMemory, mvpcMatrix);

        // Draw all the scene, but for this example it's just a single cube.
        commandBuffer.draw(QUAD_VERTEX_COUNT, 1, 0, 0);
    }

private:
    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_uniformBuffer;

    glm::vec3 m_view;
};

int main()
{
    try
    {
        auto app = QuadApp(500, 500);

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

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample extends the textured cube sample by generating a sphere instead: each face of a
// cube is subdivided into a grid, and every vertex position is normalized onto the unit sphere
// (a "cubed sphere"). The resulting per-vertex normal is just the normalized position, giving a
// smoothly shaded sphere. The mesh is drawn with an index buffer since the subdivided grid shares
// vertices between adjacent triangles within each face. The sphere is textured with a cubemap
// built from six face images, sampled in the fragment shader using the object-space normal as the
// lookup direction, so no texture coordinates or face indices are needed per vertex.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spock/loader.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include "lodepng.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct SphereVertex
{
    static spock::VertexFormat::Attributes attributes()
    {
        return {
            { vk::Format::eR32G32B32Sfloat, offsetof(SphereVertex, normal) }
        };
    }

    glm::vec3 normal;
};

static const glm::vec3 xPos{ 1.0f,  0.0f,  0.0f};
static const glm::vec3 xNeg{-1.0f,  0.0f,  0.0f};
static const glm::vec3 yPos{ 0.0f,  1.0f,  0.0f};
static const glm::vec3 yNeg{ 0.0f, -1.0f,  0.0f};
static const glm::vec3 zPos{ 0.0f,  0.0f,  1.0f};
static const glm::vec3 zNeg{ 0.0f,  0.0f, -1.0f};

// One face of the source cube, described by its outward normal and the two axes that span it.
// axisU/axisV are chosen so that cross(axisU, axisV) == normal for every face, which keeps the
// triangle winding consistent across all six faces without hand-tuning each one individually.
struct CubeFace
{
    glm::vec3 normal;
    glm::vec3 axisU;
    glm::vec3 axisV;
};

static const CubeFace CUBE_FACES[] = {
    {zPos, xPos, yNeg},
    {zNeg, xNeg, yNeg},
    {xPos, zNeg, yNeg},
    {xNeg, zPos, yNeg},
    {yPos, xNeg, zNeg},
    {yNeg, xNeg, zPos},
};

static constexpr uint32_t SPHERE_SUBDIVISIONS{24};

// Subdivides each face of a cube into a subdivisions x subdivisions grid and normalizes every
// vertex position onto the unit sphere.
static void generateSphereMesh(
    uint32_t subdivisions,
    std::vector<SphereVertex>& vertices,
    std::vector<uint32_t>& indices)
{
    uint32_t verticesPerEdge = subdivisions + 1;
    vertices.reserve(std::size(CUBE_FACES) * static_cast<size_t>(verticesPerEdge) * verticesPerEdge);
    indices.reserve(std::size(CUBE_FACES) * static_cast<size_t>(subdivisions) * subdivisions * 6);

    for (uint32_t faceIndex = 0; faceIndex < std::size(CUBE_FACES); ++faceIndex)
    {
        CubeFace const& face = CUBE_FACES[faceIndex];
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        for (uint32_t row = 0; row < verticesPerEdge; ++row)
        {
            const float v = (static_cast<float>(row) / subdivisions) * 2.0f - 1.0f;
            for (uint32_t col = 0; col < verticesPerEdge; ++col)
            {
                const float u = (static_cast<float>(col) / subdivisions) * 2.0f - 1.0f;

                glm::vec3 cubePos = face.normal + face.axisU * u + face.axisV * v;

                vertices.push_back(SphereVertex{cubePos});
            }
        }

        auto vertexIndex = [baseIndex, verticesPerEdge](uint32_t row, uint32_t col)
        {
            return baseIndex + row * verticesPerEdge + col;
        };

        for (uint32_t row = 0; row < subdivisions; ++row)
        {
            for (uint32_t col = 0; col < subdivisions; ++col)
            {
                uint32_t a = vertexIndex(row, col);
                uint32_t b = vertexIndex(row + 1, col);
                uint32_t c = vertexIndex(row, col + 1);
                uint32_t d = vertexIndex(row + 1, col + 1);

                indices.push_back(a);
                indices.push_back(c);
                indices.push_back(b);

                indices.push_back(d);
                indices.push_back(b);
                indices.push_back(c);
            }
        }
    }
}

static const std::string CUBEMAP_PATH = std::string(SPOCK_DIR) + "/assets/textures/";

static const std::array<std::string, spock::CUBEMAP_FACE_COUNT> CUBEMAP_FACES {
    CUBEMAP_PATH + "xpos.png",
    CUBEMAP_PATH + "xneg.png",
    CUBEMAP_PATH + "ypos.png",
    CUBEMAP_PATH + "yneg.png",
    CUBEMAP_PATH + "zpos.png",
    CUBEMAP_PATH + "zneg.png"
};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 itModel;
    float radius;
    float spherical;
} pc;

layout (location = 0) in vec3 cubePos;

layout (location = 0) out vec3 outTexDir;
layout (location = 1) out vec3 outNormal;

void main()
{
  vec3 normal = normalize(cubePos);
  vec3 pos = mix(cubePos, normal, pc.spherical);
  outTexDir = normal;
  outNormal = (pc.itModel * vec4(pos, 0.0)).xyz;
  gl_Position = pc.mvp * (vec4(pos, 1.0) * pc.radius);
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform samplerCube texSampler;

layout (location = 0) in vec3 texDir;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec4 outColor;

void main()
{
  vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
  vec3 lightDif = vec3(1.0);
  vec3 lightAmb = vec3(0.3);
  vec3 litColor = lightAmb + lightDif * max(dot(normalize(normal), lightDir), 0.0);
  vec3 tex = texture(texSampler, texDir).rgb;
  outColor = vec4(tex * litColor, 1.0);
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
    glm::mat4x4 itModel;
    float radius;
    float spherical;
};

class TexturedSphereRenderer : public spock::Renderer
{
public:
    TexturedSphereRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.0f, 0.0f, 0.0f, 1.0},
            {1.0f, 0},
            true)
    {
        // Generate the sphere geometry and upload it into a vertex and index buffer.
        std::vector<SphereVertex> vertices;
        std::vector<uint32_t> indices;
        generateSphereMesh(SPHERE_SUBDIVISIONS, vertices, indices);
        m_indexCount = static_cast<uint32_t>(indices.size());

        m_vertexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            vertices.size() * sizeof(SphereVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), vertices.data(), vertices.size());

        m_indexBuffer = spock::BufferWrapper(
            m_physicalDevice,
            m_device,
            indices.size() * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eIndexBuffer);
        spock::copyToDevice(m_indexBuffer.deviceMemory(), indices.data(), indices.size());

        m_cubemap = spock::Loader::cubemap(
            *this,
            m_presenter->graphicsQueue(),
            CUBEMAP_FACES);

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            m_device,
            vk::ShaderStageFlagBits::eFragment,
            {vk::DescriptorType::eCombinedImageSampler});

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, *m_descriptorSetLayout, pushConstantRange }));

        m_descriptorPool = spock::createDescriptorPool(
            m_device,
            { {vk::DescriptorType::eCombinedImageSampler, 1} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, { m_descriptorPool, *m_descriptorSetLayout }).front());

        vk::DescriptorImageInfo imageInfo(
            m_cubemap.sampler,
            m_cubemap.image.imageView(),
            vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet writeDescriptorSet(
            m_descriptorSet,
            0,
            0,
            vk::DescriptorType::eCombinedImageSampler,
            imageInfo);
        m_device.updateDescriptorSets(writeDescriptorSet, nullptr);

        createPipeline();
    }

    void update(glm::mat4x4 const& viewProjClip)
    {
        m_viewProjClip = viewProjClip;
    }

protected:
    void createPipeline()
    {
        // Create the shaders.
        glslang::InitializeProcess();
        vk::raii::ShaderModule vertexShader{nullptr};
        vk::raii::ShaderModule fragmentShader{nullptr};
        try
        {
            vertexShader = spock::compileShader(m_device, vk::ShaderStageFlagBits::eVertex, VERTEX_SHADER_SOURCE);
            fragmentShader = spock::compileShader(m_device, vk::ShaderStageFlagBits::eFragment, FRAGMENT_SHADER_SOURCE);
        }
        catch (...)
        {
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        const vk::PipelineShaderStageCreateFlags shaderStageCreateFlags{};
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
            {shaderStageCreateFlags, vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"},
            {shaderStageCreateFlags, vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"},
        };

        // Finally create the graphics pipeline.
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_device,
            shaderStagesInfo,
            m_pipelineLayout,
            m_renderPass.renderPass(),
            spock::VertexFormatWrapper<SphereVertex>());
    }

    void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) override
    {
        // Bind the pipeline, texture descriptor set, and vertex/index buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });
        commandBuffer.bindIndexBuffer(m_indexBuffer.buffer(), 0, vk::IndexType::eUint32);

        // Update the push constants.
        static const glm::mat4x4 invTransModel{ 1.0f };
        static const float radius = 1.0f;
        static const float spherical = 1.0f;
        PushConstants pushConstants{ m_viewProjClip, invTransModel, radius, spherical };
        spock::pushConstants(commandBuffer, m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, pushConstants);

        // Draw all the scene, but for this example it's just a single sphere.
        commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
    }

private:
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_graphicsPipeline{nullptr};

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSet m_descriptorSet{nullptr};

    spock::BufferWrapper m_vertexBuffer;
    spock::BufferWrapper m_indexBuffer;
    uint32_t m_indexCount{0};
    spock::CubemapWrapper m_cubemap;

    glm::mat4x4 m_viewProjClip{};
};

class TexturedSphereApp : public spock::App
{
public:
    TexturedSphereApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Textured Sphere",
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
        return std::make_unique<TexturedSphereRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
        TexturedSphereRenderer* renderer = static_cast<TexturedSphereRenderer*>(m_renderer.get());

        vk::Offset2D cursor = m_window.cursorPosition();
        if (m_window.isMouseButtonPressed(spock::MouseButton::Left))
        {
            static const float sensitivity = 0.005f;
            m_camera.update(glm::vec2(
                static_cast<float>(m_previousCursor.x - cursor.x) * sensitivity,
                static_cast<float>(cursor.y - m_previousCursor.y) * sensitivity));
        }
        m_previousCursor = cursor;
        renderer->update(m_camera.viewProjMatrix(m_window.extents()));
    }

    vk::Offset2D m_previousCursor{};
    spock::OrbitCamera m_camera{ glm::vec3(0.0f), 5.0f, 5.0f };
};

int main()
{
    return spock::runApp<TexturedSphereApp>(500, 500);
}

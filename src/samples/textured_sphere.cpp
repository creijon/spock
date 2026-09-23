// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample extends the textured cube sample by generating a sphere instead: each face of a
// cube is subdivided into a grid, and every vertex position is normalized onto the unit sphere
// (a "cubed sphere"). The resulting per-vertex normal is just the normalized position, giving a
// smoothly shaded sphere. The mesh is drawn with an index buffer since the subdivided grid shares
// vertices between adjacent triangles within each face. Each of the six faces is textured with
// its own image (an array of combined image samplers bound to a single descriptor binding), with
// a per-vertex face index selecting which array element the fragment shader samples from.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include "lodepng.h"

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
            { vk::Format::eR32G32B32A32Sfloat, offsetof(SphereVertex, pos) },
            { vk::Format::eR32G32Sfloat, offsetof(SphereVertex, uv) },
            { vk::Format::eR32G32B32Sfloat, offsetof(SphereVertex, normal) },
            { vk::Format::eR32Uint, offsetof(SphereVertex, faceIndex) }
        };
    }

    glm::vec4 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    uint32_t faceIndex;
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
    {zPos, xPos, yPos},
    {zNeg, yPos, xPos},
    {xPos, yPos, zPos},
    {xNeg, zPos, yPos},
    {yPos, zPos, xPos},
    {yNeg, xPos, zPos},
};

static constexpr uint32_t SPHERE_SUBDIVISIONS{24};

// Subdivides each face of a cube into a subdivisions x subdivisions grid and normalizes every
// vertex position onto the unit sphere. Each face keeps its own planar [0,1] UV range, the same
// scheme the flat textured cube uses, so the texture wraps around the sphere as six patches.
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
            float v = static_cast<float>(row) / subdivisions;
            for (uint32_t col = 0; col < verticesPerEdge; ++col)
            {
                float u = static_cast<float>(col) / subdivisions;

                glm::vec3 cubePos = face.normal + face.axisU * (u * 2.0f - 1.0f) + face.axisV * (v * 2.0f - 1.0f);
                glm::vec3 spherePos = glm::normalize(cubePos);

                vertices.push_back(SphereVertex{glm::vec4(spherePos, 1.0f), glm::vec2(1.0f - u, v), spherePos, faceIndex});
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
                indices.push_back(b);
                indices.push_back(c);

                indices.push_back(c);
                indices.push_back(b);
                indices.push_back(d);
            }
        }
    }
}

static constexpr uint32_t FACE_TEXTURE_COUNT{6};

// One texture per cube face, in the same order as CUBE_FACES (zPos, zNeg, xPos, xNeg, yPos, yNeg).
static const std::array<std::string, FACE_TEXTURE_COUNT> FACE_TEXTURE_FILENAMES{
    "posz.png", "negz.png", "posx.png", "negx.png", "posy.png", "negy.png"
};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 itModel;
} pc;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in uint faceIndex;

layout (location = 0) out vec2 outUv;
layout (location = 1) out vec3 outNormal;
layout (location = 2) flat out uint outFaceIndex;

void main()
{
  outUv = uv;
  outNormal = (pc.itModel * vec4(normal, 0.0)).xyz;
  outFaceIndex = faceIndex;
  gl_Position = pc.mvp * pos;
}
)";

static const std::string FRAGMENT_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (binding = 0) uniform sampler2D texSamplers[6];

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 normal;
layout (location = 2) flat in uint faceIndex;

layout (location = 0) out vec4 outColor;

void main()
{
  vec3 lightDir = normalize(vec3(1.0, -1.0, 0.5));
  vec3 lightDif = vec3(1.0);
  vec3 lightAmb = vec3(0.2);
  vec3 litColor = lightAmb + lightDif * max(dot(normalize(normal), lightDir), 0.0);
  outColor = vec4(texture(texSamplers[nonuniformEXT(faceIndex)], uv).rgb * litColor, 1.0);
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
    glm::mat4x4 itModel;
};

// Decodes the PNG at path and uploads it into a new TextureWrapper, using a one-time
// command buffer submission on the given queue.
static spock::TextureWrapper loadTexture(
    vk::raii::PhysicalDevice const &physicalDevice,
    vk::raii::Device const &device,
    vk::raii::CommandPool const &commandPool,
    vk::raii::Queue const &queue,
    std::string const &path)
{
    std::vector<unsigned char> pixels;
    unsigned width = 0;
    unsigned height = 0;
    unsigned error = lodepng::decode(pixels, width, height, path);
    if (error)
    {
        throw std::runtime_error("Failed to load texture '" + path + "': " + lodepng_error_text(error));
    }

    spock::TextureWrapper texture(physicalDevice, device, vk::Extent2D(width, height));

    spock::oneTimeSubmit(
        device,
        commandPool,
        queue,
        [&](vk::CommandBuffer commandBuffer)
        {
            texture.setImage(
                commandBuffer,
                [&pixels](void *data, vk::Extent2D const &extent)
                {
                    std::memcpy(data, pixels.data(), static_cast<size_t>(extent.width) * extent.height * 4);
                });
        });

    return texture;
}

static void const* requiredDeviceFeatures(vk::raii::Instance const& instance)
{
	// Check for extensions for sampling the face texture array with a per-fragment index.
    vk::raii::PhysicalDevice physicalDevice = vk::raii::PhysicalDevices(instance).front();

    auto supported = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
    if (!supported.get<vk::PhysicalDeviceFeatures2>().features.shaderSampledImageArrayDynamicIndexing ||
        !supported.get<vk::PhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing)
    {
        throw std::runtime_error(
            "This device does not support the sampler array indexing features required by the textured sphere sample.");
    }

    static vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features> enabledFeatures;
    enabledFeatures.get<vk::PhysicalDeviceFeatures2>().features.shaderSampledImageArrayDynamicIndexing = true;
    enabledFeatures.get<vk::PhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing = true;

    return &enabledFeatures.get<vk::PhysicalDeviceFeatures2>();
}

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
            {0.2f, 0.2f, 0.3f, 1.0},
            {1.0f, 0},
            true,
            {},
            requiredDeviceFeatures(instance))
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

        for (uint32_t i = 0; i < FACE_TEXTURE_COUNT; ++i)
        {
            std::string path = std::string(SPOCK_DIR) + "/assets/textures/" + FACE_TEXTURE_FILENAMES[i];
            m_faceTextures[i] = loadTexture(m_physicalDevice, m_device, m_commandPool, m_presenter->graphicsQueue(), path);
        }

        // A single binding holding an array of FACE_TEXTURE_COUNT combined image samplers, rather
        // than createDescriptorSetLayout's usual one-binding-per-entry layout, since the fragment
        // shader indexes into them as texSamplers[faceIndex].
        vk::DescriptorSetLayoutBinding textureArrayBinding(
            0,
            vk::DescriptorType::eCombinedImageSampler,
            FACE_TEXTURE_COUNT,
            vk::ShaderStageFlagBits::eFragment);
        m_descriptorSetLayout = vk::raii::DescriptorSetLayout(
            m_device,
            vk::DescriptorSetLayoutCreateInfo({}, textureArrayBinding));

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, { {}, *m_descriptorSetLayout, pushConstantRange }));

        m_descriptorPool = spock::createDescriptorPool(
            m_device,
            { {vk::DescriptorType::eCombinedImageSampler, FACE_TEXTURE_COUNT} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(m_device, { m_descriptorPool, *m_descriptorSetLayout }).front());

        std::array<vk::DescriptorImageInfo, FACE_TEXTURE_COUNT> imageInfos;
        for (uint32_t i = 0; i < FACE_TEXTURE_COUNT; ++i)
        {
            imageInfos[i] = vk::DescriptorImageInfo(
                m_faceTextures[i].sampler(),
                m_faceTextures[i].image().imageView(),
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }
        vk::WriteDescriptorSet writeDescriptorSet(
            m_descriptorSet,
            0,
            0,
            FACE_TEXTURE_COUNT,
            vk::DescriptorType::eCombinedImageSampler,
            imageInfos.data());
        m_device.updateDescriptorSets(writeDescriptorSet, nullptr);

        createPipeline();
    }

    void setTransform(glm::mat4x4 const &transform)
    {
        m_world = transform;
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
        static const glm::vec3 target(0.0f, 0.0f, 0.0f);
        static const glm::vec3 view(-4.0f, 0.0f, 4.0f);
        static const glm::vec3 up(0.0f, -1.0f, 0.0f);
        static const glm::mat4x4 invTransModel = glm::transpose(glm::inverse(m_world));
        glm::mat4x4 modelViewProj = spock::viewProjClipMatrix(m_extents, view, target, up) * m_world;
        PushConstants pushConstants{modelViewProj, invTransModel};
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
    std::array<spock::TextureWrapper, FACE_TEXTURE_COUNT> m_faceTextures;

    glm::mat4x4 m_world{};
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
        using Seconds = std::chrono::duration<double>;
        double angle = std::chrono::duration_cast<Seconds>(m_time).count();

        TexturedSphereRenderer* renderer = static_cast<TexturedSphereRenderer*>(m_renderer.get());

        const float radius = 5.0f;
        glm::mat4x4 world{ 1.0f };

        world = glm::rotate(world, float(angle), glm::vec3(0.0f, 1.0f, 0.0f));

        renderer->setTransform(world);
    }
};

int main()
{
    return spock::runApp<TexturedSphereApp>(500, 500);
}

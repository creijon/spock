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
            { vk::Format::eR32G32B32A32Sfloat, offsetof(SphereVertex, pos) },
            { vk::Format::eR32G32B32Sfloat, offsetof(SphereVertex, normal) }
        };
    }

    glm::vec4 pos;
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
            float v = static_cast<float>(row) / subdivisions;
            for (uint32_t col = 0; col < verticesPerEdge; ++col)
            {
                float u = static_cast<float>(col) / subdivisions;

                glm::vec3 cubePos = face.normal + face.axisU * (u * 2.0f - 1.0f) + face.axisV * (v * 2.0f - 1.0f);
                glm::vec3 spherePos = glm::normalize(cubePos);

                vertices.push_back(SphereVertex{glm::vec4(spherePos, 1.0f), spherePos});
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

static constexpr uint32_t CUBEMAP_FACE_COUNT{6};

static const std::array<std::string, CUBEMAP_FACE_COUNT> CUBEMAP_FACES{"xpos.png", "xneg.png", "ypos.png", "yneg.png", "zpos.png", "zneg.png"};

static const std::string VERTEX_SHADER_SOURCE = R"(
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 itModel;
} pc;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec3 outTexDir;
layout (location = 1) out vec3 outNormal;

void main()
{
  outTexDir = normal;
  outNormal = (pc.itModel * vec4(normal, 0.0)).xyz;
  gl_Position = pc.mvp * pos;
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
  vec3 lightAmb = vec3(0.2);
  vec3 litColor = lightAmb + lightDif * max(dot(normalize(normal), lightDir), 0.0);
  vec3 tex = texture(texSampler, texDir).rgb;
  outColor = vec4(tex * litColor, 1.0);
}
)";

struct PushConstants
{
    glm::mat4x4 mvp;
    glm::mat4x4 itModel;
};

// A cubemap image (six array layers with a cube view) and the sampler used to read it.
struct Cubemap
{
    spock::ImageWrapper image;
    vk::raii::Sampler sampler{nullptr};
};

// Decodes the six face PNGs in CUBEMAP_FACES and uploads them into the layers of a new cubemap
// image, via a staging buffer and a one-time command buffer submission on the given queue.
static Cubemap loadCubemap(
    vk::raii::PhysicalDevice const &physicalDevice,
    vk::raii::Device const &device,
    vk::raii::CommandPool const &commandPool,
    vk::raii::Queue const &queue,
    std::string const &directory)
{
    std::array<std::vector<unsigned char>, CUBEMAP_FACE_COUNT> facePixels;
    unsigned size = 0;
    for (uint32_t i = 0; i < CUBEMAP_FACE_COUNT; ++i)
    {
        std::string path = directory + CUBEMAP_FACES[i];
        unsigned width = 0;
        unsigned height = 0;
        unsigned error = lodepng::decode(facePixels[i], width, height, path);
        if (error)
        {
            throw std::runtime_error("Failed to load texture '" + path + "': " + lodepng_error_text(error));
        }
        if (width != height || (i > 0 && width != size))
        {
            throw std::runtime_error("Cubemap face '" + path + "' must be square and the same size as the other faces");
        }
        size = width;
    }

    vk::DeviceSize faceBytes = static_cast<vk::DeviceSize>(size) * size * 4;
    spock::BufferWrapper stagingBuffer(physicalDevice, device, faceBytes * CUBEMAP_FACE_COUNT, vk::BufferUsageFlagBits::eTransferSrc);
    uint8_t *staging = static_cast<uint8_t *>(stagingBuffer.deviceMemory().mapMemory(0, faceBytes * CUBEMAP_FACE_COUNT));
    for (uint32_t i = 0; i < CUBEMAP_FACE_COUNT; ++i)
    {
        std::memcpy(staging + i * faceBytes, facePixels[i].data(), faceBytes);
    }
    stagingBuffer.deviceMemory().unmapMemory();

    Cubemap cubemap;
    cubemap.image = spock::ImageWrapper(
        physicalDevice,
        device,
        vk::Format::eR8G8B8A8Unorm,
        vk::Extent2D(size, size),
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eUndefined,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageAspectFlagBits::eColor,
        CUBEMAP_FACE_COUNT,
        vk::ImageCreateFlagBits::eCubeCompatible,
        vk::ImageViewType::eCube);

    spock::oneTimeSubmit(
        device,
        commandPool,
        queue,
        [&](vk::CommandBuffer commandBuffer)
        {
            spock::setImageLayout(
                commandBuffer,
                cubemap.image.image(),
                cubemap.image.format(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                CUBEMAP_FACE_COUNT);

            // The faces are tightly packed in the staging buffer, so one copy covers all six layers.
            vk::BufferImageCopy copyRegion(
                0,
                size,
                size,
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, CUBEMAP_FACE_COUNT),
                vk::Offset3D(0, 0, 0),
                vk::Extent3D(size, size, 1));
            commandBuffer.copyBufferToImage(
                *stagingBuffer.buffer(),
                *cubemap.image.image(),
                vk::ImageLayout::eTransferDstOptimal,
                copyRegion);

            spock::setImageLayout(
                commandBuffer,
                cubemap.image.image(),
                cubemap.image.format(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                CUBEMAP_FACE_COUNT);
        });

    cubemap.sampler = vk::raii::Sampler(
        device,
        {{},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f,
        false,
        16.0f,
        false,
        vk::CompareOp::eNever,
        0.0f,
        0.0f,
        vk::BorderColor::eFloatOpaqueBlack});

    return cubemap;
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

        m_cubemap = loadCubemap(
            m_physicalDevice,
            m_device,
            m_commandPool,
            m_presenter->graphicsQueue(),
            std::string(SPOCK_DIR) + "/assets/textures/");

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
        PushConstants pushConstants{ m_viewProjClip, invTransModel};
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
    Cubemap m_cubemap;

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

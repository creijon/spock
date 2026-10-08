// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// Project six cubemap faces into Splat-compatible degree-three RGB SH, then
// reconstruct their colour on the same sphere mesh as textured_sphere.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/creators.hpp"
#include "spock/helpers.hpp"
#include "spherical_harmonics.hpp"
#include "spherical_harmonics_shaders.hpp"
#include "stb_image.h"
#include "spock/math.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <stdexcept>
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
// Together with the index order below, these axes keep triangle winding consistent.
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

static const std::array<std::string, 6> CUBEMAP_FACES {
    CUBEMAP_PATH + "xpos.png",
    CUBEMAP_PATH + "xneg.png",
    CUBEMAP_PATH + "ypos.png",
    CUBEMAP_PATH + "yneg.png",
    CUBEMAP_PATH + "zpos.png",
    CUBEMAP_PATH + "zneg.png"
};

// Decode only on the CPU: the renderer uploads the 16 coefficients, not a cubemap.
static spherical_harmonics::Coefficients loadSphericalHarmonics()
{
    using Pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    std::vector<Pixels> faces;
    int faceSize = 0;
    for (auto const& path : CUBEMAP_FACES)
    {
        int width = 0, height = 0, channels = 0;
        Pixels pixels(stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free);
        if (!pixels)
            throw std::runtime_error("Failed to load cubemap face: " + path);
        if (width <= 0 || width != height || (faceSize && width != faceSize))
            throw std::runtime_error("Cubemap faces must be square and equally sized: " + path);
        faceSize = width;
        faces.push_back(std::move(pixels));
    }
    return spherical_harmonics::projectCubemap(static_cast<size_t>(faceSize),
        [&](size_t face, size_t x, size_t y) {
            auto pixel = faces[face].get() + (y * static_cast<size_t>(faceSize) + x) * 4;
            return glm::dvec3(pixel[0], pixel[1], pixel[2]) / 255.0;
        });
}

struct PushConstants { glm::mat4x4 mvp; };

class SphericalHarmonicsSphereRenderer : public spock::Renderer
{
public:
    SphericalHarmonicsSphereRenderer(
        std::shared_ptr<const spock::Foundry> const &foundry,
        vk::Extent2D const& extents)
        : spock::Renderer(
            foundry,
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
            foundry,
            vertices.size() * sizeof(SphereVertex),
            vk::BufferUsageFlagBits::eVertexBuffer);
        spock::copyToDevice(m_vertexBuffer.deviceMemory(), vertices.data(), vertices.size());

        m_indexBuffer = spock::BufferWrapper(
            foundry,
            indices.size() * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eIndexBuffer);
        spock::copyToDevice(m_indexBuffer.deviceMemory(), indices.data(), indices.size());

        const auto coefficients = loadSphericalHarmonics();
        m_coefficients = spock::BufferWrapper(foundry, sizeof(coefficients), vk::BufferUsageFlagBits::eUniformBuffer);
        spock::copyToDevice(m_coefficients.deviceMemory(), coefficients.data(), coefficients.size());

        m_descriptorSetLayout = spock::createDescriptorSetLayout(
            foundry->device(),
            vk::ShaderStageFlagBits::eFragment,
            {vk::DescriptorType::eUniformBuffer});

        vk::PushConstantRange pushConstantRange{
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants)};

        m_pipelineLayout = std::move(vk::raii::PipelineLayout(foundry->device(), { {}, *m_descriptorSetLayout, pushConstantRange }));

        m_descriptorPool = spock::createDescriptorPool(
            foundry->device(),
            { {vk::DescriptorType::eUniformBuffer, 1} });
        m_descriptorSet = std::move(vk::raii::DescriptorSets(foundry->device(), { m_descriptorPool, *m_descriptorSetLayout }).front());

        vk::DescriptorBufferInfo bufferInfo(m_coefficients.buffer(), 0, sizeof(spherical_harmonics::Coefficients));
        vk::WriteDescriptorSet writeDescriptorSet(m_descriptorSet, 0, 0,
            vk::DescriptorType::eUniformBuffer, {}, bufferInfo);
        foundry->device().updateDescriptorSets(writeDescriptorSet, nullptr);

        createPipeline();
    }

    ~SphericalHarmonicsSphereRenderer() override
    {
        waitIdle();
    }

    void update(glm::mat4x4 const& viewProjClip)
    {
        m_viewProjClip = viewProjClip;
    }

protected:
    void createPipeline()
    {
        // Create the shaders.
        auto vertShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eVertex, spherical_harmonics::vertexShader);
        auto fragShader = spock::compileShader(m_foundry->device(), vk::ShaderStageFlagBits::eFragment, spherical_harmonics::fragmentShader);

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesInfo{
            {{}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main"},
            {{}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main"},
        };

        // Finally create the graphics pipeline.
        m_graphicsPipeline = spock::createGraphicsPipeline(
            m_foundry->device(),
            shaderStagesInfo,
            m_pipelineLayout,
            m_renderPass,
            spock::VertexFormatWrapper<SphereVertex>());
    }

    void render(spock::FrameState &frame) override
    {
        auto& commandBuffer = frame.commandBuffer;

        // Bind the pipeline, SH coefficient descriptor, and vertex/index buffers.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSet}, nullptr);
        commandBuffer.bindVertexBuffers(0, { m_vertexBuffer.buffer() }, { 0 });
        commandBuffer.bindIndexBuffer(m_indexBuffer.buffer(), 0, vk::IndexType::eUint32);

        // Update the push constants.
        PushConstants pushConstants{ m_viewProjClip };
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
    spock::BufferWrapper m_coefficients;

    glm::mat4x4 m_viewProjClip{};
};

class SphericalHarmonicsSphereApp : public spock::App
{
public:
    SphericalHarmonicsSphereApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App(
            "Spherical Harmonics Sphere",
            windowWidth,
            windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer() override
    {
        return std::make_unique<SphericalHarmonicsSphereRenderer>(m_foundry, m_window.extents());
    }

    void update() override
    {
        SphericalHarmonicsSphereRenderer* renderer = static_cast<SphericalHarmonicsSphereRenderer*>(m_renderer.get());

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
    return spock::runApp<SphericalHarmonicsSphereApp>(500, 500);
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample demonstrates spock::DebugLines. It draws 32 wireframe cubes orbiting around a sphere
// along random paths.

#include "spock/app.hpp"
#include "spock/camera.hpp"
#include "spock/debug_lines.hpp"
#include "spock/math.hpp"
#include "spock/renderer.hpp"

#include <array>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <random>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32_t CUBE_COUNT = 32;
    constexpr float ORBIT_RADIUS = 4.0f;
    constexpr float CUBE_HALF_SIZE = 0.35f;
    constexpr uint32_t SPHERE_SEGMENTS = 48;
    constexpr float CAMERA_DISTANCE = 10.0f;
    constexpr float CAMERA_HEIGHT = 3.0f;

    // Pairs of cube corner indices, where corners are numbered so that indices differing in a
    // single bit are adjacent -- that set of pairs is exactly the 12 edges of the cube.
    constexpr std::array<std::pair<int, int>, 12> CUBE_EDGES{{
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
        {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}
    }};

    glm::vec3 cubeCorner(int index, float halfSize)
    {
        return glm::vec3(
            (index & 1) ? halfSize : -halfSize,
            (index & 2) ? halfSize : -halfSize,
            (index & 4) ? halfSize : -halfSize);
    }

    // Returns an arbitrary unit vector perpendicular to axis, used as the starting point of a
    // cube's orbit around that axis.
    glm::vec3 perpendicular(glm::vec3 const& axis)
    {
        glm::vec3 reference = std::fabs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        return glm::normalize(glm::cross(axis, reference));
    }

    // A saturated color for hue in [0, 1), spacing CUBE_COUNT of these evenly gives each cube a
    // distinct, easily told-apart color.
    glm::vec4 hueToColor(float hue)
    {
        float h = hue * 6.0f;
        float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
        glm::vec3 rgb;
        if (h < 1.0f) rgb = {1.0f, x, 0.0f};
        else if (h < 2.0f) rgb = {x, 1.0f, 0.0f};
        else if (h < 3.0f) rgb = {0.0f, 1.0f, x};
        else if (h < 4.0f) rgb = {0.0f, x, 1.0f};
        else if (h < 5.0f) rgb = {x, 0.0f, 1.0f};
        else rgb = {1.0f, 0.0f, x};
        return glm::vec4(rgb, 1.0f);
    }

    struct OrbitingCube
    {
        glm::vec3 orbitAxis;
        glm::vec3 orbitStart;
        float orbitSpeed;
        glm::vec3 spinAxis;
        float spinSpeed;
        glm::vec4 color;
    };

    std::vector<OrbitingCube> makeOrbitingCubes(uint32_t count)
    {
        std::mt19937 rng(1); // fixed seed so the demo looks the same on every run

        std::uniform_real_distribution<float> orbitSpeedDist(0.2f, 0.5f);
        std::uniform_real_distribution<float> spinSpeedDist(0.5f, 2.0f);
        std::uniform_real_distribution<float> signDist(-1.0f, 1.0f);

        std::vector<OrbitingCube> cubes;
        cubes.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            glm::vec3 orbitAxis = spock::randomUnitVector(rng);
            float orbitSign = signDist(rng) < 0.0f ? -1.0f : 1.0f;
            float spinSign = signDist(rng) < 0.0f ? -1.0f : 1.0f;

            cubes.push_back(OrbitingCube{
                orbitAxis,
                perpendicular(orbitAxis) * ORBIT_RADIUS,
                orbitSpeedDist(rng) * orbitSign,
                spock::randomUnitVector(rng),
                spinSpeedDist(rng) * spinSign,
                hueToColor(static_cast<float>(i) / static_cast<float>(count))});
        }
        return cubes;
    }

    // A faint wireframe of three orthogonal rings outlining the sphere the cubes orbit around.
    std::vector<glm::vec3> makeSphereLines(float radius, uint32_t segments)
    {
        std::vector<glm::vec3> lines;
        lines.reserve(segments * 3);

        auto addRing = [&](auto const& pointAt)
        {
            for (uint32_t i = 0; i < segments; ++i)
            {
                float a0 = static_cast<float>(i) / static_cast<float>(segments) * glm::two_pi<float>();
                float a1 = static_cast<float>(i + 1) / static_cast<float>(segments) * glm::two_pi<float>();
                lines.emplace_back(pointAt(a0));
                lines.emplace_back(pointAt(a1));
            }
        };

        addRing([&](float a) { return radius * glm::vec3(std::cos(a), std::sin(a), 0.0f); });
        addRing([&](float a) { return radius * glm::vec3(std::cos(a), 0.0f, std::sin(a)); });
        addRing([&](float a) { return radius * glm::vec3(0.0f, std::cos(a), std::sin(a)); });

        return lines;
    }

    const glm::vec4 SPHERE_LINE_COLOR{0.4f, 0.4f, 0.45f, 1.0f};
}

class DebugLinesRenderer : public spock::Renderer
{
public:
    DebugLinesRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.02f, 0.02f, 0.05f, 1.0f},
            {1.0f, 0})
        , m_debugLines(m_physicalDevice, m_device, m_renderPass.renderPass())
        , m_cubes(makeOrbitingCubes(CUBE_COUNT))
        , m_sphereLines(makeSphereLines(ORBIT_RADIUS, SPHERE_SEGMENTS))
    {
    }

protected:
    void render(vk::raii::CommandBuffer const& commandBuffer, std::chrono::microseconds time) override
    {
        using Seconds = std::chrono::duration<float>;
        float t = std::chrono::duration_cast<Seconds>(time).count();

        m_debugLines.clear();

        for (size_t i = 0; i < m_sphereLines.size(); i += 2)
        {
            m_debugLines.addLine(m_sphereLines[i], m_sphereLines[i + 1], SPHERE_LINE_COLOR);
        }

        for (auto const& cube : m_cubes)
        {
            glm::quat orbitRotation = glm::angleAxis(t * cube.orbitSpeed, cube.orbitAxis);
            glm::vec3 center = orbitRotation * cube.orbitStart;

            glm::quat spinRotation = glm::angleAxis(t * cube.spinSpeed, cube.spinAxis);

            glm::vec3 corners[8];
            for (int i = 0; i < 8; ++i)
            {
                corners[i] = center + spinRotation * cubeCorner(i, CUBE_HALF_SIZE);
            }

            for (auto const& edge : CUBE_EDGES)
            {
                m_debugLines.addLine(corners[edge.first], corners[edge.second], cube.color);
            }
        }

        float cameraAngle = t * 0.15f;
        glm::vec3 eye(
            std::sin(cameraAngle) * CAMERA_DISTANCE,
            CAMERA_HEIGHT,
            std::cos(cameraAngle) * CAMERA_DISTANCE);
        glm::mat4x4 viewProjection = spock::viewProjClipMatrix(
            m_extents, eye, glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f));

        m_debugLines.draw(commandBuffer, viewProjection);
    }

private:
    spock::DebugLines m_debugLines;
    std::vector<OrbitingCube> m_cubes;
    std::vector<glm::vec3> m_sphereLines;
};

class DebugLinesApp : public spock::App
{
public:
    DebugLinesApp(uint32_t windowWidth, uint32_t windowHeight)
        : spock::App("DebugLines", windowWidth, windowHeight)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<DebugLinesRenderer>(instance, std::move(windowSurface), extents);
    }

    void update() override
    {
    }
};

int main()
{
    return spock::runApp<DebugLinesApp>(800, 800);
}

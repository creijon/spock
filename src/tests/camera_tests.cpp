// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
    // Independently re-derive the expected matrix using the same formula
    // documented for viewProjMatrix, so the test doesn't just re-implement
    // the function under test verbatim from memory.
    glm::mat4x4 expectedViewProjClip(
        vk::Extent2D const &extent,
        glm::vec3 const &eye,
        glm::vec3 const &center,
        glm::vec3 const &up,
        float fov,
        float zNear,
        float zFar)
    {
        float aspect = extent.height > 0
            ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
            : 1.0f;

        glm::mat4x4 view = glm::lookAt(eye, center, up);
        glm::mat4x4 projection = glm::perspective(glm::radians(fov), aspect, zNear, zFar);
        // clang-format off
        glm::mat4x4 clip{
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f};
        // clang-format on

        return clip * projection * view;
    }

    bool matAlmostEqual(glm::mat4x4 const &a, glm::mat4x4 const &b, float eps = 1e-4f)
    {
        for (int col = 0; col < 4; col++)
        {
            for (int row = 0; row < 4; row++)
            {
                if (std::abs(a[col][row] - b[col][row]) > eps)
                {
                    return false;
                }
            }
        }
        return true;
    }
} // namespace

TEST_CASE("viewProjMatrix matches the documented view/projection/clip composition", "[camera]")
{
    vk::Extent2D extent(1920, 1080);
    glm::vec3 eye(3.0f, 2.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 actual = spock::viewProjMatrix(extent, eye, center, up, 60.0f, 0.5f, 200.0f);
    glm::mat4x4 expected = expectedViewProjClip(extent, eye, center, up, 60.0f, 0.5f, 200.0f);

    CHECK(matAlmostEqual(actual, expected));
}

TEST_CASE("viewProjMatrix uses the default fov/near/far when not specified", "[camera]")
{
    vk::Extent2D extent(800, 600);
    glm::vec3 eye(0.0f, 0.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 actual = spock::viewProjMatrix(extent, eye, center, up);
    glm::mat4x4 expected = expectedViewProjClip(extent, eye, center, up, 45.0f, 0.1f, 1000.0f);

    CHECK(matAlmostEqual(actual, expected));
}

TEST_CASE("viewProjMatrix falls back to a square aspect ratio for a zero-height extent", "[camera]")
{
    vk::Extent2D zeroHeightExtent(800, 0);
    vk::Extent2D squareExtent(800, 800);
    glm::vec3 eye(1.0f, 1.0f, 1.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    // A zero-height extent should behave exactly like an explicit 1:1 aspect
    // ratio, per the "aspect = 1.0f" fallback in the implementation.
    glm::mat4x4 zeroHeightResult = spock::viewProjMatrix(zeroHeightExtent, eye, center, up);
    glm::mat4x4 squareResult = spock::viewProjMatrix(squareExtent, eye, center, up);

    CHECK(matAlmostEqual(zeroHeightResult, squareResult));
}

TEST_CASE("viewProjMatrix flips the projected Y axis for Vulkan clip space", "[camera]")
{
    // Looking down -Z with a point straight above eye level should end up
    // with a negative clip-space Y after the Vulkan Y-flip is applied.
    vk::Extent2D extent(100, 100);
    glm::vec3 eye(0.0f, 0.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 mvp = spock::viewProjMatrix(extent, eye, center, up);
    glm::vec4 pointAboveCenter = mvp * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    CHECK(pointAboveCenter.y < 0.0f);
}

TEST_CASE("OrbitCamera starts behind its focus point", "[camera]")
{
    spock::OrbitCamera camera(glm::vec3(1.0f, 2.0f, 3.0f), 5.0f, 5.0f, 60.0f);
    vk::Extent2D extent(100, 100);

    glm::mat4x4 actual = camera.viewProjMatrix(extent);
    glm::mat4x4 expected = expectedViewProjClip(
        extent,
        glm::vec3(1.0f, 2.0f, 8.0f),
        glm::vec3(1.0f, 2.0f, 3.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        60.0f,
        0.1f,
        1000.0f);

    CHECK(matAlmostEqual(actual, expected));
}

TEST_CASE("OrbitCamera mouse delta orbits around its focus point", "[camera]")
{
    spock::OrbitCamera camera(glm::vec3(0.0f), 5.0f, 5.0f);
    camera.update(glm::vec2(glm::half_pi<float>(), 0.0f));

    glm::mat4x4 actual = camera.viewProjMatrix(vk::Extent2D(100, 100));
    glm::mat4x4 expected = expectedViewProjClip(
        vk::Extent2D(100, 100),
        glm::vec3(5.0f, 0.0f, 0.0f),
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        45.0f,
        0.1f,
        1000.0f);

    CHECK(matAlmostEqual(actual, expected));
}

#include "spock/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
    // Independently re-derive the expected matrix using the same formula
    // documented for viewProjClipMatrix, so the test doesn't just re-implement
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

TEST_CASE("viewProjClipMatrix matches the documented view/projection/clip composition", "[camera]")
{
    vk::Extent2D extent(1920, 1080);
    glm::vec3 eye(3.0f, 2.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 actual = spock::viewProjClipMatrix(extent, eye, center, up, 60.0f, 0.5f, 200.0f);
    glm::mat4x4 expected = expectedViewProjClip(extent, eye, center, up, 60.0f, 0.5f, 200.0f);

    CHECK(matAlmostEqual(actual, expected));
}

TEST_CASE("viewProjClipMatrix uses the default fov/near/far when not specified", "[camera]")
{
    vk::Extent2D extent(800, 600);
    glm::vec3 eye(0.0f, 0.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 actual = spock::viewProjClipMatrix(extent, eye, center, up);
    glm::mat4x4 expected = expectedViewProjClip(extent, eye, center, up, 45.0f, 0.1f, 1000.0f);

    CHECK(matAlmostEqual(actual, expected));
}

TEST_CASE("viewProjClipMatrix falls back to a square aspect ratio for a zero-height extent", "[camera]")
{
    vk::Extent2D zeroHeightExtent(800, 0);
    vk::Extent2D squareExtent(800, 800);
    glm::vec3 eye(1.0f, 1.0f, 1.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    // A zero-height extent should behave exactly like an explicit 1:1 aspect
    // ratio, per the "aspect = 1.0f" fallback in the implementation.
    glm::mat4x4 zeroHeightResult = spock::viewProjClipMatrix(zeroHeightExtent, eye, center, up);
    glm::mat4x4 squareResult = spock::viewProjClipMatrix(squareExtent, eye, center, up);

    CHECK(matAlmostEqual(zeroHeightResult, squareResult));
}

TEST_CASE("viewProjClipMatrix flips the projected Y axis for Vulkan clip space", "[camera]")
{
    // Looking down -Z with a point straight above eye level should end up
    // with a negative clip-space Y after the Vulkan Y-flip is applied.
    vk::Extent2D extent(100, 100);
    glm::vec3 eye(0.0f, 0.0f, 5.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4x4 mvp = spock::viewProjClipMatrix(extent, eye, center, up);
    glm::vec4 pointAboveCenter = mvp * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    CHECK(pointAboveCenter.y < 0.0f);
}

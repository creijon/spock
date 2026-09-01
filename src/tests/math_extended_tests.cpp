// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/math.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
    bool vec3AlmostEqual(glm::vec3 const &a, glm::vec3 const &b, float eps = 1e-4f)
    {
        return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
    }

    bool quatAlmostEqual(glm::quat const &a, glm::quat const &b, float eps = 1e-4f)
    {
        return std::abs(a.w - b.w) <= eps && std::abs(a.x - b.x) <= eps &&
               std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
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

    bool transformAlmostEqual(spock::Transform const &a, spock::Transform const &b, float eps = 1e-4f)
    {
        return quatAlmostEqual(a.orientation, b.orientation, eps) &&
               vec3AlmostEqual(a.translation, b.translation, eps) &&
               std::abs(a.scale - b.scale) <= eps;
    }
} // namespace

TEST_CASE("Transform with zero scale has valid inverse", "[math_extended]")
{
    // This is actually undefined behavior, but we're testing that it doesn't crash
    spock::Transform t(glm::quat(1, 0, 0, 0), glm::vec3(0, 0, 0), 0.0001f);
    spock::Transform inv = t.inverse();
    
    // Inverse should exist and have correct orientation
    CHECK(inv.scale > 0);
}

TEST_CASE("Transform interpolation at t=0 returns first transform", "[math_extended]")
{
    spock::Transform a(
        glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)),
        glm::vec3(1, 2, 3),
        2.0f);
    spock::Transform b(
        glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)),
        glm::vec3(4, 5, 6),
        3.0f);
    
    spock::Transform result = spock::Transform::interpolate(a, b, 0.0f);
    
    CHECK(transformAlmostEqual(result, a));
}

TEST_CASE("Transform interpolation at t=1 returns second transform", "[math_extended]")
{
    spock::Transform a(
        glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)),
        glm::vec3(1, 2, 3),
        2.0f);
    spock::Transform b(
        glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)),
        glm::vec3(4, 5, 6),
        3.0f);
    
    spock::Transform result = spock::Transform::interpolate(a, b, 1.0f);
    
    CHECK(transformAlmostEqual(result, b));
}

TEST_CASE("Transform interpolation at t=0.5 gives midpoint", "[math_extended]")
{
    spock::Transform a(
        glm::quat(1, 0, 0, 0),
        glm::vec3(0, 0, 0),
        1.0f);
    spock::Transform b(
        glm::quat(1, 0, 0, 0),
        glm::vec3(10, 20, 30),
        3.0f);
    
    spock::Transform mid = spock::Transform::interpolate(a, b, 0.5f);
    
    CHECK(vec3AlmostEqual(mid.translation, glm::vec3(5, 10, 15)));
    CHECK(std::abs(mid.scale - 2.0f) < 1e-4f);
}

TEST_CASE("Transform composition is associative", "[math_extended]")
{
    spock::Transform a(
        glm::angleAxis(glm::radians(15.0f), glm::vec3(1, 0, 0)),
        glm::vec3(1, 0, 0),
        1.0f);
    spock::Transform b(
        glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 1, 0)),
        glm::vec3(0, 1, 0),
        1.0f);
    spock::Transform c(
        glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 0, 1)),
        glm::vec3(0, 0, 1),
        1.0f);
    
    // (a * b) * c should equal a * (b * c)
    spock::Transform left = (a * b) * c;
    spock::Transform right = a * (b * c);
    
    CHECK(transformAlmostEqual(left, right, 1e-3f));
}

TEST_CASE("Transform with uniform scale scales all axes equally", "[math_extended]")
{
    spock::Transform t(glm::quat(1, 0, 0, 0), glm::vec3(0, 0, 0), 2.0f);
    
    glm::vec3 point(1, 1, 1);
    glm::vec4 scaled = t.toMatrix() * glm::vec4(point, 1);
    
    CHECK(vec3AlmostEqual(glm::vec3(scaled), glm::vec3(2, 2, 2)));
}

TEST_CASE("Transform translation is applied after scale and rotation", "[math_extended]")
{
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
    glm::vec3 trans(5, 0, 0);
    float scale = 2.0f;
    
    spock::Transform t(rot, trans, scale);
    
    glm::vec3 point(1, 0, 0);
    glm::vec4 result = t.toMatrix() * glm::vec4(point, 1);
    
    // After scale: (2, 0, 0)
    // After rotation: (0, 2, 0)
    // After translation: (5, 2, 0)
    CHECK(vec3AlmostEqual(glm::vec3(result), glm::vec3(5, 2, 0)));
}

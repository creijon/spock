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

TEST_CASE("Transform default-constructs to the identity transform", "[math]")
{
    spock::Transform identity;

    CHECK(quatAlmostEqual(identity.orientation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)));
    CHECK(vec3AlmostEqual(identity.translation, glm::vec3(0.0f)));
    CHECK(identity.scale == 1.0f);
    CHECK(matAlmostEqual(identity.toMatrix(), glm::mat4x4(1.0f)));
}

TEST_CASE("Transform::toMatrix applies scale, then rotation, then translation to a point", "[math]")
{
    glm::quat orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 translation(1.0f, 2.0f, 3.0f);
    float scale = 2.0f;
    spock::Transform transform(orientation, translation, scale);

    glm::vec3 point(1.0f, 0.0f, 0.0f);
    glm::vec4 transformed = transform.toMatrix() * glm::vec4(point, 1.0f);

    glm::vec3 expected = orientation * (scale * point) + translation;
    CHECK(vec3AlmostEqual(glm::vec3(transformed), expected));
}

TEST_CASE("Identity is the identity element for operator* and operator*=", "[math]")
{
    spock::Transform identity;
    spock::Transform t(
        glm::angleAxis(glm::radians(30.0f), glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f))),
        glm::vec3(4.0f, -1.0f, 2.0f),
        1.5f);

    CHECK(transformAlmostEqual(identity * t, t));
    CHECK(transformAlmostEqual(t * identity, t));

    spock::Transform composed = t;
    composed *= identity;
    CHECK(transformAlmostEqual(composed, t));
}

TEST_CASE("operator* applies the right-hand transform first, then the left-hand transform", "[math]")
{
    spock::Transform a(
        glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::vec3(1.0f, 0.0f, 0.0f),
        2.0f);
    spock::Transform b(
        glm::angleAxis(glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::vec3(0.0f, 2.0f, 0.0f),
        0.5f);

    spock::Transform combined = a * b;

    glm::vec3 point(3.0f, -1.0f, 2.0f);
    glm::vec4 viaCombinedMatrix = combined.toMatrix() * glm::vec4(point, 1.0f);
    glm::vec4 viaSeparateMatrices = a.toMatrix() * (b.toMatrix() * glm::vec4(point, 1.0f));

    CHECK(vec3AlmostEqual(glm::vec3(viaCombinedMatrix), glm::vec3(viaSeparateMatrices)));
}

TEST_CASE("operator*= mutates the left-hand transform to match operator*", "[math]")
{
    spock::Transform a(
        glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::vec3(-1.0f, 2.0f, 0.5f),
        1.25f);
    spock::Transform b(
        glm::angleAxis(glm::radians(70.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::vec3(2.0f, 0.0f, -3.0f),
        0.75f);

    spock::Transform expected = a * b;
    spock::Transform actual = a;
    actual *= b;

    CHECK(transformAlmostEqual(actual, expected));
}

TEST_CASE("Transform::inverse undoes the transform in both composition orders", "[math]")
{
    spock::Transform t(
        glm::angleAxis(glm::radians(50.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f))),
        glm::vec3(5.0f, -2.0f, 1.0f),
        3.0f);

    spock::Transform inverse = t.inverse();

    CHECK(matAlmostEqual((t * inverse).toMatrix(), glm::mat4x4(1.0f)));
    CHECK(matAlmostEqual((inverse * t).toMatrix(), glm::mat4x4(1.0f)));
}

TEST_CASE("Transform::interpolate lerps translation/scale and slerps orientation", "[math]")
{
    spock::Transform a(
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        1.0f);
    spock::Transform b(
        glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::vec3(10.0f, 20.0f, 30.0f),
        3.0f);

    spock::Transform atStart = spock::Transform::interpolate(a, b, 0.0f);
    spock::Transform atEnd = spock::Transform::interpolate(a, b, 1.0f);
    spock::Transform atMid = spock::Transform::interpolate(a, b, 0.5f);

    CHECK(transformAlmostEqual(atStart, a));
    CHECK(transformAlmostEqual(atEnd, b));

    spock::Transform expectedMid(
        glm::slerp(a.orientation, b.orientation, 0.5f),
        glm::mix(a.translation, b.translation, 0.5f),
        glm::mix(a.scale, b.scale, 0.5f));
    CHECK(transformAlmostEqual(atMid, expectedMid));
}

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#if defined(_MSC_VER)
#pragma warning(disable : 4201) // disable warning C4201: nonstandard extension used: nameless struct/union; needed
                                // to get glm/detail/type_vec?.hpp without warnings
#elif defined(__GNUC__)
// don't know how to switch off that warning here
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include <vulkan/vulkan.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant (glm)
#endif

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace spock
{
    class Transform
    {
    public:
        Transform() = default;
        Transform(glm::quat const &orientation, glm::vec3 const &translation, float scale = 1.0f)
            : orientation(orientation), translation(translation), scale(scale)
        {
        }

        // Concatenates transforms: (lhs *= rhs) makes lhs apply rhs first, then lhs.
        Transform &operator*=(Transform const &rhs)
        {
            translation = translation + orientation * (scale * rhs.translation);
            orientation = orientation * rhs.orientation;
            scale = scale * rhs.scale;
            return *this;
        }

        // Concatenates transforms: (lhs * rhs) applies rhs first, then lhs.
        Transform operator*(Transform const &rhs) const
        {
            Transform result = *this;
            result *= rhs;
            return result;
        }

        Transform inverse() const
        {
            glm::quat invOrientation = glm::conjugate(orientation);
            float invScale = 1.0f / scale;
            return Transform(
                invOrientation,
                -invScale * (invOrientation * translation),
                invScale);
        }

        glm::mat4x4 toMatrix() const
        {
            glm::mat4x4 matrix = glm::mat4_cast(orientation);
            matrix[0] *= scale;
            matrix[1] *= scale;
            matrix[2] *= scale;
            matrix[3] = glm::vec4(translation, 1.0f);
            return matrix;
        }

        static Transform interpolate(Transform const &a, Transform const &b, float t)
        {
            return Transform(
                glm::slerp(a.orientation, b.orientation, t),
                glm::mix(a.translation, b.translation, t),
                glm::mix(a.scale, b.scale, t));
        }

        glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 translation{0.0f};
        float scale = 1.0f;
    };
} // namespace spock

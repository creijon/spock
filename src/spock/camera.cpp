// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "camera.hpp"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace spock
{
    glm::mat4x4 viewProjClipMatrix(
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
        // Vulkan clip space has inverted y and half z.
        glm::mat4x4 clip{
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f};
        // clang-format on 

        return clip * projection * view;
    }
} // namespace spock

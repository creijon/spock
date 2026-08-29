// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "camera.hpp"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace spock
{
    OrbitCamera::OrbitCamera(
        glm::vec3 const &focus,
        float distance,
        float fov,
        float zNear,
        float zFar)
        : m_focus(focus)
        , m_distance(distance)
        , m_fov(fov)
        , m_zNear(zNear)
        , m_zFar(zFar)
    {
    }

    void OrbitCamera::update(glm::vec2 const &mouseDelta)
    {
        m_yaw += mouseDelta.x;
        m_pitch = glm::clamp(m_pitch + mouseDelta.y, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);
    }

    glm::mat4x4 OrbitCamera::viewProjClipMatrix(vk::Extent2D const &extent) const
    {
        glm::vec3 direction(
            std::cos(m_pitch) * std::sin(m_yaw),
            std::sin(m_pitch),
            std::cos(m_pitch) * std::cos(m_yaw));
        glm::vec3 eye = m_focus + direction * m_distance;

        return spock::viewProjClipMatrix(
            extent,
            eye,
            m_focus,
            glm::vec3(0.0f, 1.0f, 0.0f),
            m_fov,
            m_zNear,
            m_zFar);
    }

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

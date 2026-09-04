// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "camera.hpp"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace spock
{
    OrbitCamera::OrbitCamera(
        glm::vec3 const& focus,
        float minDistance,
        float maxDistance,
        float fov,
        float zNear,
        float zFar)
        : m_focus(focus)
        , m_minDistance(minDistance)
        , m_maxDistance(maxDistance)
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

    glm::vec3 OrbitCamera::position() const
    {
        glm::vec3 eye = m_focus;

        eye.x += m_minDistance * std::cos(m_pitch) * std::sin(m_yaw);
        eye.y += m_minDistance * std::sin(m_pitch);
        eye.z += m_minDistance * std::cos(m_pitch) * std::cos(m_yaw);

        return eye;
    }

    glm::mat4x4 OrbitCamera::view() const
    {
        glm::vec3 up{0.0f, 1.0f, 0.0f};

        return glm::lookAt(position(), m_focus, up);
    }

    glm::mat4x4 OrbitCamera::projection(vk::Extent2D const &extent) const
    {
        float aspect = extent.height > 0
            ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
            : 1.0f;

        return glm::perspective(glm::radians(m_fov), aspect, m_zNear, m_zFar);
    }

    glm::mat4x4 OrbitCamera::viewProjClipMatrix(vk::Extent2D const &extent) const
    {
        // clang-format off
        // Vulkan clip space has inverted y and half z.
        static constexpr glm::mat4x4 clip{
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f};
        // clang-format on 

        return clip * projection(extent) * view();
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
        static constexpr glm::mat4x4 clip{
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f};
        // clang-format on 

        return clip * projection * view;
    }
} // namespace spock

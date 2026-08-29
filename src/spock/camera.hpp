// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "math.hpp"

namespace spock
{
    class OrbitCamera
    {
    public:
        OrbitCamera(
            glm::vec3 const &focus,
            float distance,
            float fov = 45.0f,
            float zNear = 0.1f,
            float zFar = 1000.0f);

        void update(glm::vec2 const &mouseDelta);

        glm::mat4x4 viewProjClipMatrix(vk::Extent2D const &extent) const;

        glm::vec3 const &focus() const
        {
            return m_focus;
        }

        float distance() const
        {
            return m_distance;
        }

        float fov() const
        {
            return m_fov;
        }

    private:
        glm::vec3 m_focus;
        float m_distance;
        float m_fov;
        float m_zNear;
        float m_zFar;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };

    glm::mat4x4 viewProjClipMatrix(
        vk::Extent2D const &extent,
        glm::vec3 const &eye,
        glm::vec3 const &center,
        glm::vec3 const &up,
        float fov = 45.0f,
        float zNear = 0.1f,
        float zFar = 1000.0f);

} // namespace spock

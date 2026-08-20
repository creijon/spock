// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "math.hpp"

namespace spock
{
    glm::mat4x4 viewProjClipMatrix(
        vk::Extent2D const &extent,
        glm::vec3 const &eye,
        glm::vec3 const &center,
        glm::vec3 const &up,
        float fov = 45.0f,
        float zNear = 0.1f,
        float zFar = 1000.0f);

} // namespace spock

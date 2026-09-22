// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "math.hpp"

namespace spock
{
    glm::vec3 randomUnitVector(std::mt19937& rng)
    {
        std::normal_distribution<float> gaussian(0.0f, 1.0f);
        glm::vec3 v;
        do
        {
            v = glm::vec3(gaussian(rng), gaussian(rng), gaussian(rng));
        } while (glm::length(v) < 1e-5f);
        return glm::normalize(v);
    }
}  // namespace spock

// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT
#pragma once

#include "spock/math.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace spherical_harmonics
{
// Flattened Splat layout: sh0, sh1[0..2], sh2[0..4], sh3[0..6].
// vec4 keeps each RGB coefficient on a std140 16-byte boundary; w is unused.
using Coefficients = std::array<glm::vec4, 16>;
static_assert(sizeof(Coefficients) == 256, "SH coefficients must match the std140 shader block");

// Same real SH basis as shaders/splat.vs; direction must be a unit vector.
inline std::array<double, 16> basis(glm::dvec3 const& direction)
{
    const double x = direction.x, y = direction.y, z = direction.z;
    constexpr double SH_C0 = 0.28209479177387814;
    constexpr double SH_C1 = 0.4886025119029199;
    constexpr double SH_C2_0 = 1.0925484305920792;
    constexpr double SH_C2_1 = -1.0925484305920792;
    constexpr double SH_C2_2 = 0.31539156525252005;
    constexpr double SH_C2_3 = -1.0925484305920792;
    constexpr double SH_C2_4 = 0.5462742152960396;
    constexpr double SH_C3_0 = -0.5900435899266435;
    constexpr double SH_C3_1 = 2.890611442640554;
    constexpr double SH_C3_2 = -0.4570457994644658;
    constexpr double SH_C3_3 = 0.3731763325901154;
    constexpr double SH_C3_4 = -0.4570457994644658;
    constexpr double SH_C3_5 = 1.445305721320277;
    constexpr double SH_C3_6 = -0.5900435899266435;
    return {
        SH_C0,
        -SH_C1 * y,
        SH_C1 * z,
        -SH_C1 * x,
        SH_C2_0 * x * y,
        SH_C2_1 * y * z,
        SH_C2_2 * (2.0 * z * z - x * x - y * y),
        SH_C2_3 * x * z,
        SH_C2_4 * (x * x - y * y),
        SH_C3_0 * y * (3.0 * x * x - y * y),
        SH_C3_1 * x * y * z,
        SH_C3_2 * y * (4.0 * z * z - x * x - y * y),
        SH_C3_3 * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y),
        SH_C3_4 * x * (4.0 * z * z - x * x - y * y),
        SH_C3_5 * z * (x * x - y * y),
        SH_C3_6 * x * (x * x - 3.0 * y * y)
    };
}

// Vulkan samplerCube orientation, with u/v in [-1,1], v increasing down the image.
// Faces are +X, -X, +Y, -Y, +Z, -Z; no image flip is applied.
inline glm::dvec3 cubemapDirection(size_t face, double u, double v)
{
    const std::array<glm::dvec3, 6> directions{{
        {1, -v, -u}, {-1, -v, u}, {u, 1, v},
        {u, -1, -v}, {u, -v, 1}, {-u, -v, -1}
    }};
    return glm::normalize(directions.at(face));
}

inline double texelSolidAngle(double u0, double v0, double u1, double v1)
{
    auto area = [](double u, double v) {
        return std::atan2(u * v, std::sqrt(u * u + v * v + 1.0));
    };
    return area(u1, v1) - area(u0, v1) - area(u1, v0) + area(u0, v0);
}

// Convolve RGB with each real SH basis over the sphere. Exact texel solid angles
// avoid over-weighting cube corners; the basis is sampled at each texel centre.
// sample(face, x, y) returns RGB in [0,1], matching Loader's UNORM cubemap.
// Project RGB - 0.5 because Splat adds 0.5 after evaluating its coefficients.
// No Lambertian cosine kernel is applied: these encode directional colour.
template<typename Sample>
Coefficients projectCubemap(size_t size, Sample const& sample)
{
    if (size == 0)
        throw std::invalid_argument("Cubemap faces must be nonempty");
    std::array<glm::dvec3, 16> integrals{};
    const double step = 2.0 / static_cast<double>(size);
    for (size_t face = 0; face < 6; ++face)
        for (size_t y = 0; y < size; ++y)
            for (size_t x = 0; x < size; ++x)
            {
                const double u0 = -1.0 + x * step, v0 = -1.0 + y * step;
                const auto values = basis(cubemapDirection(face, u0 + step * 0.5, v0 + step * 0.5));
                const double weight = texelSolidAngle(u0, v0, u0 + step, v0 + step);
                const glm::dvec3 rgb = glm::dvec3(sample(face, x, y)) - glm::dvec3(0.5);
                for (size_t i = 0; i < integrals.size(); ++i)
                    integrals[i] += rgb * (values[i] * weight);
            }
    Coefficients result{};
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = glm::vec4(glm::vec3(integrals[i]), 0.0f);
    return result;
}
} // namespace spherical_harmonics

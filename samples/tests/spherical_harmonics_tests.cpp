// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "samples/spherical_harmonics.hpp"
#include "samples/spherical_harmonics_shaders.hpp"
#include "spock/shaders.hpp"
#include "gpu_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace sh = spherical_harmonics;
using Catch::Approx;

TEST_CASE("Cubemap texel areas cover the sphere", "[spherical_harmonics]")
{
    constexpr size_t size = 32;
    double area = 0;
    for (size_t y = 0; y < size; ++y)
        for (size_t x = 0; x < size; ++x)
        {
            const double u = -1.0 + 2.0 * x / size, v = -1.0 + 2.0 * y / size;
            const double weight = sh::texelSolidAngle(u, v, u + 2.0 / size, v + 2.0 / size);
            REQUIRE(weight > 0);
            area += weight * 6;
        }
    CHECK(area == Approx(4 * std::acos(-1.0)).margin(1e-12));
}

TEST_CASE("Cubemap directions follow Vulkan face order and orientation", "[spherical_harmonics]")
{
    const std::array<glm::dvec3, 6> axes{{{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}};
    const std::array<glm::dvec3, 6> corners{{{1,-1,-1}, {-1,-1,1}, {1,1,1}, {1,-1,-1}, {1,-1,1}, {-1,-1,-1}}};
    for (size_t face = 0; face < 6; ++face)
    {
        CHECK(glm::length(sh::cubemapDirection(face, 0, 0) - axes[face]) < 1e-12);
        CHECK(glm::length(sh::cubemapDirection(face, 1, 1) - glm::normalize(corners[face])) < 1e-12);
    }
}

TEST_CASE("Constant cubemap projects to Splat DC with its colour bias", "[spherical_harmonics]")
{
    const glm::dvec3 colour{0.2, 0.6, 0.9};
    const auto coefficients = sh::projectCubemap(32, [&](size_t, size_t, size_t) { return colour; });
    constexpr double c0 = 0.28209479177387814;
    for (size_t channel = 0; channel < 3; ++channel)
    {
        CHECK(coefficients[0][channel] == Approx((colour[channel] - 0.5) / c0).margin(1e-6));
        for (size_t i = 1; i < 16; ++i)
            CHECK(coefficients[i][channel] == Approx(0).margin(1e-6));
    }
    CHECK_THROWS_AS(sh::projectCubemap(0, [](size_t, size_t, size_t) { return glm::vec3(0); }), std::invalid_argument);
}

TEST_CASE("Directional RGB gradients project to the Splat degree-one signs and order", "[spherical_harmonics]")
{
    constexpr size_t size = 48;
    const auto coefficients = sh::projectCubemap(size, [](size_t face, size_t x, size_t y) {
        const auto d = sh::cubemapDirection(face, -1.0 + 2.0 * (x + 0.5) / size, -1.0 + 2.0 * (y + 0.5) / size);
        return glm::dvec3(0.5) + d * 0.2;
    });
    constexpr double amplitude = 0.2 / 0.4886025119029199;
    for (size_t i = 0; i < 16; ++i)
        for (size_t channel = 0; channel < 3; ++channel)
        {
            double expected = 0;
            if (i == 3 && channel == 0) expected = -amplitude;
            if (i == 1 && channel == 1) expected = -amplitude;
            if (i == 2 && channel == 2) expected = amplitude;
            CHECK(coefficients[i][channel] == Approx(expected).margin(0.0002));
        }
}

TEST_CASE("Cubemap integration recovers all sixteen orthonormal SH modes", "[spherical_harmonics]")
{
    constexpr size_t size = 48;
    for (size_t mode = 0; mode < 16; ++mode)
    {
        INFO("SH mode " << mode);
        const auto coefficients = sh::projectCubemap(size, [mode](size_t face, size_t x, size_t y) {
            const auto d = sh::cubemapDirection(face, -1.0 + 2.0 * (x + 0.5) / size, -1.0 + 2.0 * (y + 0.5) / size);
            return glm::dvec3(0.5 + 0.2 * sh::basis(d)[mode]);
        });
        for (size_t i = 0; i < 16; ++i)
            CHECK(coefficients[i].r == Approx(i == mode ? 0.2 : 0).margin(0.0002));
    }
}

TEST_CASE("Spherical harmonics sample shaders compile", "[spherical_harmonics][gpu]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No headless Vulkan device available");
    const auto& device = fixture->foundry->device();
    CHECK_NOTHROW(spock::compileShader(device, vk::ShaderStageFlagBits::eVertex, sh::vertexShader));
    CHECK_NOTHROW(spock::compileShader(device, vk::ShaderStageFlagBits::eFragment, sh::fragmentShader));
}

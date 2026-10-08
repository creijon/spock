// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace spherical_harmonics
{
inline const std::string vertexShader = R"(#version 450
layout(push_constant) uniform PushConstants { mat4 mvp; } pc;
layout(location = 0) in vec3 cubePos;
layout(location = 0) out vec3 outDirection;
void main()
{
    outDirection = normalize(cubePos);
    gl_Position = pc.mvp * vec4(outDirection, 1.0);
}
)";
inline const std::string fragmentShader = R"(#version 450
layout(std140, binding = 0) uniform SphericalHarmonics { vec4 coefficients[16]; } sh;
layout(location = 0) in vec3 direction;
layout(location = 0) out vec4 outColor;
// Basis and reconstruction follow splat.vs, with float RGB coefficients in std140.
const float SH_C0 = 0.28209479177387814;
const float SH_C1 = 0.4886025119029199;
const float SH_C2_0 = 1.0925484305920792;
const float SH_C2_1 = -1.0925484305920792;
const float SH_C2_2 = 0.31539156525252005;
const float SH_C2_3 = -1.0925484305920792;
const float SH_C2_4 = 0.5462742152960396;
const float SH_C3_0 = -0.5900435899266435;
const float SH_C3_1 = 2.890611442640554;
const float SH_C3_2 = -0.4570457994644658;
const float SH_C3_3 = 0.3731763325901154;
const float SH_C3_4 = -0.4570457994644658;
const float SH_C3_5 = 1.445305721320277;
const float SH_C3_6 = -0.5900435899266435;
vec3 sphericalHarmonicsToRgb(vec3 viewVec)
{
    float x = viewVec.x;
    float y = viewVec.y;
    float z = viewVec.z;

    vec3 rgb = SH_C0 * sh.coefficients[0].rgb;

    rgb += -SH_C1 * y * sh.coefficients[1].rgb;
    rgb += SH_C1 * z * sh.coefficients[2].rgb;
    rgb += -SH_C1 * x * sh.coefficients[3].rgb;

    rgb += SH_C2_0 * x * y * sh.coefficients[4].rgb;
    rgb += SH_C2_1 * y * z * sh.coefficients[5].rgb;
    rgb += SH_C2_2 * (2.0 * z * z - x * x - y * y) * sh.coefficients[6].rgb;
    rgb += SH_C2_3 * x * z * sh.coefficients[7].rgb;
    rgb += SH_C2_4 * (x * x - y * y) * sh.coefficients[8].rgb;

    rgb += SH_C3_0 * y * (3.0 * x * x - y * y) * sh.coefficients[9].rgb;
    rgb += SH_C3_1 * x * y * z * sh.coefficients[10].rgb;
    rgb += SH_C3_2 * y * (4.0 * z * z - x * x - y * y) * sh.coefficients[11].rgb;
    rgb += SH_C3_3 * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y) * sh.coefficients[12].rgb;
    rgb += SH_C3_4 * x * (4.0 * z * z - x * x - y * y) * sh.coefficients[13].rgb;
    rgb += SH_C3_5 * z * (x * x - y * y) * sh.coefficients[14].rgb;
    rgb += SH_C3_6 * x * (x * x - 3.0 * y * y) * sh.coefficients[15].rgb;

    return clamp(rgb + vec3(0.5), 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(direction);
    // Same surface lighting as textured_sphere; SH replaces its texture colour.
    float lighting = 0.3 + max(dot(normal, normalize(vec3(1.0, 1.0, 0.5))), 0.0);
    outColor = vec4(sphericalHarmonicsToRgb(normal) * lighting, 1.0);
}
)";
}

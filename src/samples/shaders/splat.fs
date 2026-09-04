#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 inCoord;
layout (location = 1) in vec3 inColor;
layout (location = 2) in float inOpacity;

layout (location = 0) out vec4 outColor;

const float SPLAT_EXTENT = 3.0;

void main()
{
    float power = -0.5 * SPLAT_EXTENT * SPLAT_EXTENT * dot(inCoord, inCoord);
    if (power < -9.0) {
        discard;
    }

    float alpha = min(0.99, inOpacity * exp(power));
    if (alpha < 0.001) {
        discard;
    }

    outColor = vec4(inColor, alpha);
}

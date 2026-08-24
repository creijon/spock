#version 330 core

layout (location = 0) in vec2 splatCoord;
layout (location = 1) in vec4 splatColor;

layout (location = 0) out vec4 fragColor;

const float SPLAT_EXTENT = 3.0;

void main() {
    float power = -0.5 * SPLAT_EXTENT * SPLAT_EXTENT * dot(splatCoord, splatCoord);
    if (power < -9.0) {
        discard;
    }

    float alpha = min(0.99, splatColor.a * exp(power));
    if (alpha < 0.001) {
        discard;
    }

    fragColor = vec4(splatColor.rgb, alpha);
}

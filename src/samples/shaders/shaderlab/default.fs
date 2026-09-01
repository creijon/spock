// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  vec4 iMouse;        // image/buffer xy = current pixel coords (if LMB is down). zw = click pixel
  vec3 iResolution;   // image/buffer The viewport resolution (z is pixel aspect ratio, usually 1.0)
  float iTime;        // image/sound/buffer Current time in seconds
  int iFrame;         // image/buffer Current frame
} pc;

layout (location = 0) in vec2 fragCoord;

layout (location = 0) out vec4 fragColor;

void main()
{
  fragColor = vec4(fragCoord.x / pc.iResolution.x, fragCoord.y / pc.iResolution.y, 0.5 + 0.5 * sin(pc.iTime), 1.0);
}

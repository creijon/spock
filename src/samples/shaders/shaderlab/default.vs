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

layout (location = 0) in vec2 pos;

layout (location = 0) out vec2 fragCoord;

void main()
{
  // The coordinates are in pixel units, ranging from 0.5 to resolution-0.5
  vec2 uv = (pos.xy + 1.0) * 0.5;
  fragCoord = uv * pc.iResolution.xy + 0.5;
  gl_Position = vec4(pos, 1.0, 1.0);
}

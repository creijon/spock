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

float intersectSphere(in vec3 ro, in vec3 rd, in vec3 sc, in float sr)
{
  vec3 oc = ro - sc;
  float b = dot(oc, rd);
  float c = dot(oc, oc) - sr * sr;
  float h = b * b - c;
  if (h < 0.0) return -1.0;
  return -b - sqrt(h);
}

void main()
{
	vec2 uv = (fragCoord.xy / pc.iResolution.xy) * 2.0 - 1.0;
	uv.x *= pc.iResolution.x /  pc.iResolution.y;

	vec3 dir = normalize(vec3(uv, 1.0));
  vec3 origin = vec3(0.0);

  vec3 centre = vec3(0.0, 0.0, 1.0);
  float radius = 0.5;

  float t = intersectSphere(origin, dir, centre, radius);

  if (t == -1.0)
  {
    fragColor = vec4(0.1, 0.3, 0.6, 1.0);
    return;
  }

  vec3 p = origin + dir * t;
  vec3 n = normalize(p - centre);

  vec3 lightDir = normalize(vec3(-0.5, -0.5, -0.5));
  float diff = dot(n, lightDir);

  vec3 diffuseColor = vec3(0.9, 0.7, 0.1);
  vec3 ambientColor = vec3(0.0, 0.1, 0.2);

  fragColor = vec4(mix(ambientColor, diffuseColor, diff), 1.0);
}

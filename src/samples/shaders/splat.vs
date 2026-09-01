#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform buffer
{
  mat4 view;
  mat4 proj;
  vec2 viewport;
  mat4 mvp;
} ubo;

// Per-vertex (binding 0, shared quad)
layout(location = 0) in vec2 inPos;

// Per-instance (binding 1, one entry per sprite)
layout(location = 1) in vec3 inInstanceCentroid;
layout(location = 2) in vec4 inInstanceRotation;
layout(location = 3) in vec3 inInstanceScale;
layout(location = 4) in float inInstanceOpacity;

void main()
{
//    gl_Position = ubo.mvp * vec4(inPos.x, inPos.y, 0.0, 1.0);

    vec4 centerClip = ubo.proj * ubo.view * vec4(inInstanceCentroid, 1.0);
    vec3 ndc = centerClip.xyz / centerClip.w;

    vec2 axis0 = vec2(10.0, 0.0);
    vec2 axis1 = vec2(0.0, 10.0);

    vec2 pixelOffset = inPos.x * axis0 + inPos.y * axis1;
    vec2 ndcOffset = pixelOffset / (ubo.viewport * 0.5);

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
}

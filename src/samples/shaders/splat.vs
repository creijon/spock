#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
    vec2 viewport;
} pc;

struct InstanceData
{
    float position[3];
    float rotation[4];
    float scale[3];
    float opacity;
    float sh0[3];
};

layout (std430, binding = 0) readonly buffer InstanceBuffer
{
  InstanceData instances[];
} instanceBuffer;

// Per-vertex (binding 0, shared quad)
layout(location = 0) in vec2 inVertPos;

// Sorted splats (binding 1, one entry per splat)
layout(location = 1) in float inSplatZDist;
layout(location = 2) in uint inSplatIndex;

layout (location = 0) out vec4 outFragColor;

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

vec3 floatToVec3(float v[3])
{
    return vec3(v[0], v[1], v[2]);
}

vec4 floatToVec3(float v[4])
{
    return vec4(v[0], v[1], v[2], v[3]);
}

void main()
{
    InstanceData i = instanceBuffer.instances[inSplatIndex];
  
    vec3 pos = floatToVec3(i.position);
    vec4 centerClip = pc.proj * pc.view * vec4(pos, 1.0);
    vec3 ndc = centerClip.xyz / centerClip.w;

    vec2 axis0 = vec2(1.0, 0.0);
    vec2 axis1 = vec2(0.0, 1.0);

    vec2 pixelOffset = inVertPos.x * axis0 + inVertPos.y * axis1;
    vec2 ndcOffset = pixelOffset / (pc.viewport * 0.5);

    vec3 rgb = SH_C0 * floatToVec3(i.sh0);

    outFragColor = vec4(rgb, 1.0 / (1.0 + exp(-i.opacity)));

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
}

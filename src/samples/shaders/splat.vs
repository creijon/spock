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
    vec3 position;
    vec4 rotation;
    vec3 scale;
    float opacity;
    vec3 sh0;
};

layout (std430, binding = 0) readonly buffer InstanceBuffer
{
  InstanceData instances[];
} instanceBuffer;

// Per-vertex (binding 0, shared quad)
layout(location = 0) in vec2 inVertPos;

// Per-instance (binding 1, one entry per sprite)
layout(location = 1) in uint inInstance;

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

void main()
{
    InstanceData i = instanceBuffer.instances[inInstance];
  
    vec4 centerClip = pc.proj * pc.view * vec4(i.position, 1.0);
    vec3 ndc = centerClip.xyz / centerClip.w;

    vec2 axis0 = vec2(1.0, 0.0);
    vec2 axis1 = vec2(0.0, 1.0);

    vec2 pixelOffset = inVertPos.x * axis0 + inVertPos.y * axis1;
    vec2 ndcOffset = pixelOffset / (pc.viewport * 0.5);

    vec3 rgb = SH_C0 * i.sh0;

    outFragColor = vec4(rgb, 1.0 / (1.0 + exp(-i.opacity)));

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
}

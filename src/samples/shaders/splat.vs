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

const float SPLAT_EXTENT = 3.0;

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

vec4 floatToVec4(float v[4])
{
    return vec4(v[0], v[1], v[2], v[3]);
}

mat3 quaternionToMat3(vec4 quaternion) {
    vec4 q = normalize(quaternion);
    float w = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;

    return mat3(
        1.0 - 2.0 * y * y - 2.0 * z * z,
        2.0 * x * y + 2.0 * w * z,
        2.0 * x * z - 2.0 * w * y,
        2.0 * x * y - 2.0 * w * z,
        1.0 - 2.0 * x * x - 2.0 * z * z,
        2.0 * y * z + 2.0 * w * x,
        2.0 * x * z + 2.0 * w * y,
        2.0 * y * z - 2.0 * w * x,
        1.0 - 2.0 * x * x - 2.0 * y * y);
}

mat3 buildCovariance3D(InstanceData i)
{
    vec3 sigma = exp(floatToVec3(i.scale));
    mat3 rotationMatrix = quaternionToMat3(floatToVec4(i.rotation));
    mat3 scaleMatrix = mat3(
        sigma.x, 0.0, 0.0,
        0.0, sigma.y, 0.0,
        0.0, 0.0, sigma.z);
    mat3 rs = rotationMatrix * scaleMatrix;
    mat3 covariance = rs * transpose(rs);

    mat3 previewFlipY = mat3(
        1.0, 0.0, 0.0,
        0.0, -1.0, 0.0,
        0.0, 0.0, 1.0);
    return previewFlipY * covariance * previewFlipY;
}

mat3 projectCovarianceToScreen(vec3 centerCamera, mat3 covariance3D)
{
    float focalX = pc.proj[0][0] * pc.viewport.x * 0.5;
    float focalY = pc.proj[1][1] * pc.viewport.y * 0.5;
    float z = centerCamera.z;

    mat3 jacobian = mat3(
        -focalX / z, 0.0, 0.0,
        0.0, -focalY / z, 0.0,
        focalX * centerCamera.x / (z * z), focalY * centerCamera.y / (z * z), 0.0);

    mat3 viewLinear = mat3(pc.view);
    mat3 transform = jacobian * viewLinear;
    return transform * covariance3D * transpose(transform);
}

void main()
{
    InstanceData i = instanceBuffer.instances[inSplatIndex];
  
    vec3 pos = floatToVec3(i.position);
    vec4 centerClip = pc.proj * pc.view * vec4(pos, 1.0);
    vec3 centerCamera = vec3(pc.view * vec4(pos, 1.0));

    mat3 covariance2D = projectCovarianceToScreen(centerCamera, buildCovariance3D(i));

    float a = covariance2D[0][0] + 0.3;
    float b = covariance2D[1][0];
    float d = covariance2D[1][1] + 0.3;

    float determinant = a * d - b * b;
    float mid = 0.5 * (a + d);
    float radius = sqrt(max(mid * mid - determinant, 0.0));
    float lambda1 = max(mid + radius, 0.01);
    float lambda2 = max(mid - radius, 0.01);

    vec2 axisDirection1 =
        abs(b) > 0.00001 ? normalize(vec2(b, lambda1 - a))
                         : (a >= d ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    vec2 axisDirection2 = vec2(-axisDirection1.y, axisDirection1.x);

    vec2 axis1 = SPLAT_EXTENT * sqrt(lambda1) * axisDirection1;
    vec2 axis2 = SPLAT_EXTENT * sqrt(lambda2) * axisDirection2;

    vec3 ndc = centerClip.xyz / centerClip.w;

    vec2 pixelOffset = inVertPos.x * axis1 + inVertPos.y * axis2;
    vec2 ndcOffset = pixelOffset / (pc.viewport * 0.5);

    vec3 rgb = SH_C0 * floatToVec3(i.sh0);

    outFragColor = vec4(rgb, 1.0 / (1.0 + exp(-i.opacity)));

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
}

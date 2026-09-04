#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Constants
const float SPLAT_EXTENT = 3.0;
const uint SH_DEGREE0_COUNT = 1;
const uint SH_DEGREE1_COUNT = 3;
const uint SH_DEGREE2_COUNT = 5;
const uint SH_DEGREE3_COUNT = 7;

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

// Structs
struct SplatData
{
    float position[3];
    float rotation[4];
    float scale[3];
    float opacity;
    float sh0[3];
    float sh1[SH_DEGREE1_COUNT][3];
    float sh2[SH_DEGREE2_COUNT][3];
    float sh3[SH_DEGREE3_COUNT][3];
};

// Buffer Bindings
layout (binding = 0) uniform FrameConstants
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 viewport;
} fc;

layout (std430, binding = 1) readonly buffer SplatBuffer
{
  SplatData splats[];
} splatBuffer;

// Vertex Bindings
layout(location = 0) in vec2 inVertPos;
layout(location = 1) in float inSplatZDist;
layout(location = 2) in uint inSplatIndex;

layout (location = 0) out vec2 outCoord;
layout (location = 1) out vec3 outColor;
layout (location = 2) out float outOpacity;

// Functions
vec3 floatToVec3(float v[3])
{
    return vec3(v[0], v[1], v[2]);
}

vec4 floatToVec4(float v[4])
{
    return vec4(v[0], v[1], v[2], v[3]);
}

mat3 quatToMat3(vec4 q)
{
    return mat3(
        1.0 - 2.0 * q.y * q.y - 2.0 * q.z * q.z,
        2.0 * q.x * q.y + 2.0 * q.w * q.z,
        2.0 * q.x * q.z - 2.0 * q.w * q.y,
        2.0 * q.x * q.y - 2.0 * q.w * q.z,
        1.0 - 2.0 * q.x * q.x - 2.0 * q.z * q.z,
        2.0 * q.y * q.z + 2.0 * q.w * q.x,
        2.0 * q.x * q.z + 2.0 * q.w * q.y,
        2.0 * q.y * q.z - 2.0 * q.w * q.x,
        1.0 - 2.0 * q.x * q.x - 2.0 * q.y * q.y);
}

mat3 buildCovariance3D(vec4 rotation, vec3 scale)
{
    vec3 sigma = exp(scale);
    mat3 rotationMatrix = quatToMat3(rotation);
    mat3 scaleMatrix = mat3(
        sigma.x, 0.0, 0.0,
        0.0, sigma.y, 0.0,
        0.0, 0.0, sigma.z);
    mat3 rs = rotationMatrix * scaleMatrix;
    mat3 covariance = rs * transpose(rs);

    return covariance;
}

mat3 projectCovarianceToScreen(vec3 centreView, mat3 covariance3D)
{
    float focalX = fc.proj[0][0] * fc.viewport.x * 0.5;
    float focalY = fc.proj[1][1] * fc.viewport.y * 0.5;
    float z = centreView.z;

    mat3 jacobian = mat3(
        -focalX / z, 0.0, 0.0,
        0.0, -focalY / z, 0.0,
        focalX * centreView.x / (z * z), focalY * centreView.y / (z * z), 0.0);

    mat3 viewLinear = mat3(fc.view);
    mat3 transform = jacobian * viewLinear;
    return transform * covariance3D * transpose(transform);
}

vec3 sphericalHarmonicsToRgb(vec3 viewVec, SplatData s)
{
    float x = viewVec.x;
    float y = viewVec.y;
    float z = viewVec.z;

    vec3 rgb = SH_C0 * floatToVec3(s.sh0);

    rgb += -SH_C1 * y * floatToVec3(s.sh1[0]);
    rgb += SH_C1 * z * floatToVec3(s.sh1[1]);
    rgb += -SH_C1 * x * floatToVec3(s.sh1[2]);

    rgb += SH_C2_0 * x * y * floatToVec3(s.sh2[0]);
    rgb += SH_C2_1 * y * z * floatToVec3(s.sh2[1]);
    rgb += SH_C2_2 * (2.0 * z * z - x * x - y * y) * floatToVec3(s.sh2[2]);
    rgb += SH_C2_3 * x * z * floatToVec3(s.sh2[3]);
    rgb += SH_C2_4 * (x * x - y * y) * floatToVec3(s.sh2[4]);

    rgb += SH_C3_0 * y * (3.0 * x * x - y * y) * floatToVec3(s.sh3[0]);
    rgb += SH_C3_1 * x * y * z * floatToVec3(s.sh3[1]);
    rgb += SH_C3_2 * y * (4.0 * z * z - x * x - y * y) * floatToVec3(s.sh3[2]);
    rgb += SH_C3_3 * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y) * floatToVec3(s.sh3[3]);
    rgb += SH_C3_4 * x * (4.0 * z * z - x * x - y * y) * floatToVec3(s.sh3[4]);
    rgb += SH_C3_5 * z * (x * x - y * y) * floatToVec3(s.sh3[5]);
    rgb += SH_C3_6 * x * (x * x - 3.0 * y * y) * floatToVec3(s.sh3[6]);

    return clamp(rgb + vec3(0.5), 0.0, 1.0);
}

void main()
{
    SplatData s = splatBuffer.splats[inSplatIndex];
  
    vec3 pos = floatToVec3(s.position);
    vec4 centreClip = fc.proj * fc.view * vec4(pos, 1.0);
    vec3 centreView = vec3(fc.view * vec4(pos, 1.0));

    if (centreClip.w <= 0.0 || centreView.z >= -0.001 ||
        fc.viewport.x <= 0.0 || fc.viewport.y <= 0.0)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        outCoord = inVertPos;
        outColor = vec3(0.0);
        outOpacity = 0.0;
        return;
    }

    mat3 covariance3D = buildCovariance3D(floatToVec4(s.rotation), floatToVec3(s.scale));
    mat3 covariance2D = projectCovarianceToScreen(centreView, covariance3D);

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

    vec3 ndc = centreClip.xyz / centreClip.w;

    vec2 pixelOffset = inVertPos.x * axis1 + inVertPos.y * axis2;
    vec2 ndcOffset = pixelOffset / (fc.viewport.xy * 0.5);

    outCoord = inVertPos;
    vec3 viewVec = normalize(pos - fc.cameraPos.xyz);
    outColor = sphericalHarmonicsToRgb(viewVec, s);
    outOpacity = 1.0 / (1.0 + exp(-s.opacity));

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
}

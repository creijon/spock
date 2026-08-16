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
	vec2 uv = (fragCoord.xy / pc.iResolution.xy) * 2.0 - 1.0;
	uv.x *= pc.iResolution.x /  pc.iResolution.y;
	vec3 dir = normalize(vec3(uv, 1.0));

  float radius = 0.2;

  fragColor = vec4(dir * 0.5 + 0.5, 1.0);
}

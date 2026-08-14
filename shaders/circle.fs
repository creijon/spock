#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant) uniform PushConstants {
  vec3 iResolution;   // image/buffer The viewport resolution (z is pixel aspect ratio, usually 1.0)
  float iTime;        // image/sound/buffer Current time in seconds
  int iFrame;         // image/buffer Current frame
} pc;

layout (location = 0) in vec2 fragCoord;

layout (location = 0) out vec4 fragColor;

void main()
{
  // Normalise coordinates to 0..1 and correct for window aspect ratio.
  vec2 xy = fragCoord / pc.iResolution.xy;
  float aspect = pc.iResolution.x / pc.iResolution.y;
  xy.x = xy.x * aspect + (1.0 - aspect) * 0.5;

  // Draw the circle.
  float radius = 0.4;
  float thickness = 0.02;
  vec4 lineColor = vec4(0.5, 0.1, 0.2, 1.0);
  vec4 fillColor = vec4(0.8, 0.6, 0.1, 1.0);
  vec4 backColor = vec4(0.15, 0.05, 0.05, 1.0);

  float distance = length(xy - 0.5);
  float line = (distance < radius) ? 1.0 : 0.0;
  float fill = (distance < radius - thickness) ? 1.0 : 0.0;

  fragColor = mix(mix(backColor, lineColor, line), fillColor, fill);
}

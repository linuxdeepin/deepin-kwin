#version 140

uniform vec3      iResolution;
uniform vec2      iOffset;

uniform vec4 modulation;
uniform float saturation;

uniform float t;
uniform sampler2D sampler;

in vec2 texcoord0;
out vec4 fragColor;

float udRoundBox( vec2 p, vec2 b, float r )
{
  return length(max(abs(p)-b+r,0.0))-r;
}

void main()
{
  vec2 uv = texcoord0;
  vec4 Color = texture(sampler, uv);
  Color.a = t;

  float iRadius = 10.0;
  vec2 halfRes = 0.5 * iResolution.xy;
  vec2 startP=gl_FragCoord.xy - iOffset;

  float b = udRoundBox( startP - halfRes, halfRes, iRadius );
  float a = mix( Color.a, 0.0, smoothstep(0.0,2.0,b) );
  Color.a = a;
  fragColor = Color;  
}

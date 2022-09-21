uniform vec3      iResolution;
uniform vec2      iOffset;

uniform vec4 modulation;
uniform float saturation;

uniform float t;
uniform sampler2D sampler;

varying vec2 texcoord0;

float udRoundBox( vec2 p, vec2 b, float r )
{
  return length(max(abs(p)-b+r,0.0))-r;
}

float smoothstep(float v0, float v1, float x) {
    float t;
    t = clamp((x - v0) / (v1 - v0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

void main()
{
  vec2 uv = texcoord0;
  vec4 Color = texture2D(sampler, uv);
  Color.a = t;

  float iRadius = 10.0;
  vec2 halfRes = 0.5 * iResolution.xy;
  vec2 startP=gl_FragCoord.xy - iOffset;

  float b = udRoundBox( startP - halfRes, halfRes, iRadius );
  float a = mix( Color.a, 0.0, smoothstep(0.0, 2.0, b) );
  Color.a = a;
  gl_FragColor = Color;
}

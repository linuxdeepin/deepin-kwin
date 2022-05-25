#version 140
out vec4 fragColor;uniform vec4 geometryColor;
uniform vec3 iResolution;uniform vec2 iOffset;
float udRoundBox( vec2 p, vec2 b, float r ){return length(max(abs(p)-b+r,0.0))-r;}
void main(){float iRadius = 18.0;vec2 halfRes = 0.5 * iResolution.xy;vec2 startP=gl_FragCoord.xy - iOffset;float b = udRoundBox( startP - halfRes, halfRes, iRadius );vec4 c = mix( vec4(1.0, 1.0, 1.0, 0.2), vec4(0.0,0.0,0.0,0.0), smoothstep(0.0,1.0,b) );fragColor = c;}
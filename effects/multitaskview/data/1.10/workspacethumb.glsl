uniform sampler2D image;
uniform vec3      iResolution;
uniform vec2      iOffset;

varying vec2 texcoord0;

float udRoundBox( vec2 p, vec2 b, float r )
{
    return length(max(abs(p)-b+r,0.0))-r;
}

void main() {
    vec4 tex = texture2D(image, texcoord0);

    float iRadius = 8.0;
    vec2 halfRes = 0.5 * iResolution.xy;
    vec2 startP=gl_FragCoord.xy - iOffset;

    float b = udRoundBox( startP - halfRes, halfRes, iRadius );
    vec4 c = mix( tex, vec4(0.0,0.0,0.0,0.0), smoothstep(0.0,1.0,b) );

    gl_FragColor = c;
}

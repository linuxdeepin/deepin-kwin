#version 140
out vec4 fragColor;
uniform vec4 geometryColor;

void main()
{
    vec4 c = vec4(geometryColor.r, geometryColor.g, geometryColor.b, 0.3);
    fragColor = c;
}
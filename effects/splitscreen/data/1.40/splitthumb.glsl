#version 140
out vec4 fragColor;
uniform vec4 geometryColor;

void main() {
    vec4 color = geometryColor;

    fragColor = color;
}

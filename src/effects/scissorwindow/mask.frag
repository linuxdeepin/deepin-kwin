#version 300 es
precision highp float;

uniform sampler2D sampler, msk1;

in vec2 texcoord0;
out vec4 fragColor;

void main() {
    vec4 c = texture(sampler, texcoord0);
    vec4 m = texture(msk1, texcoord0);
    c *= m.a;
    fragColor = c;
}

// vim: set ft=glsl:

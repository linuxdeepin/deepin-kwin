#version 140

uniform sampler2D sampler, msk1;
uniform vec2 k;
uniform float clip_left, clip_right, clip_fade;
uniform float opacity;

in vec2 texcoord0;
out vec4 fragColor;

void main() {
    vec4 c = texture(sampler, texcoord0);
    if (texcoord0.t > 0.5) {
        vec2 tc = texcoord0 * k - vec2(0, k.t - 1.0);
        tc.t = 1.0 - tc.t;
        vec4 m1 = texture(msk1, tc);
        tc = 1.0 - ((texcoord0 - 1.0) * k + 1.0);
        vec4 m3 = texture(msk1, tc);
        c *= (m1.a * m3.a);
    }
    if (clip_left >= clip_right) {
        c = vec4(0, 0, 0, 0);
    } else {
        float alpha = 1.0;
        if (clip_left > 0.0)
            alpha *= smoothstep(clip_left, min(clip_left + clip_fade, clip_right), texcoord0.s);
        if (clip_right < 1.0)
            alpha *= 1.0 - smoothstep(max(clip_right - clip_fade, clip_left), clip_right, texcoord0.s);
        c *= alpha;
    }
    fragColor = c * opacity;
}

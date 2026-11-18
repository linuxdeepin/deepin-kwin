#version 140

uniform sampler2D sampler;
uniform float clip_left, clip_right, clip_fade;
uniform float opacity;
uniform float widthScale;

in vec2 texcoord0;
out vec4 fragColor;

void main() {
    float x = texcoord0.x < 0.5 ? min(texcoord0.x * widthScale, 0.5)
                               : max(1.0 - (1.0 - texcoord0.x) * widthScale, 0.5);
    vec4 c = texture(sampler, vec2(x, texcoord0.y));
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

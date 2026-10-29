#version 140
uniform vec4 color;
uniform float opacity;
uniform float borderWidth;
uniform vec4 box;
uniform vec4 cornerRadius;
in vec2 vUV;
in vec2 vPos;
out vec4 fragColor;

float sdfRoundedBox(vec2 p, vec2 center, vec2 extents, vec4 radius)
{
    vec2 pr = p - center;
    float r = pr.x > 0.0
        ? (pr.y < 0.0 ? radius.y : radius.w)
        : (pr.y < 0.0 ? radius.x : radius.z);
    vec2 q = abs(pr) - extents + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main(void)
{
    float f = sdfRoundedBox(vPos, box.xy, box.zw, cornerRadius);
    float df = fwidth(f);
    float inner = 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
    float alpha;
    if (borderWidth > 0.0) {
        float outer = 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
        float inner2 = 1.0 - clamp(0.5 + (f + borderWidth) / df, 0.0, 1.0);
        alpha = outer * (1.0 - inner2);
    } else {
        alpha = inner;
    }
    float a = color.a * opacity * alpha;
    fragColor = vec4(color.rgb * a, a);
}

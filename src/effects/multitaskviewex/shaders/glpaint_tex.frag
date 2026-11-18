uniform sampler2D tex;
uniform float opacity;
uniform vec4 box;
uniform vec4 cornerRadius;
varying vec2 vUV;
varying vec2 vPos;

float sdfRoundedBox(vec2 p, vec2 center, vec2 extents, vec4 radius)
{
    vec2 pr = p - center;
    float r = pr.x > 0.0
        ? (pr.y < 0.0 ? radius.y : radius.w)
        : (pr.y < 0.0 ? radius.x : radius.z);
    vec2 q = abs(pr) - extents + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

uniform float borderWidth;

void main(void)
{
    vec4 c = texture2D(tex, vUV);
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
    gl_FragColor = c * (opacity * alpha);
}

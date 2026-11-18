uniform sampler2D sampler;
uniform sampler2D activeShadow;
uniform float shadowProgress;
uniform vec4 inactiveShadowUV;
uniform vec2 size;
uniform float radius;
uniform float opacity;

varying vec2 texcoord0;

void main()
{
    if (shadowProgress >= 0.0) {
        vec2 uv = texcoord0 * inactiveShadowUV.xy + inactiveShadowUV.zw;
        vec4 inactive = all(equal(uv, clamp(uv, 0.0, 1.0))) ? texture2D(sampler, uv) : vec4(0.0);
        gl_FragColor = mix(inactive, texture2D(activeShadow, texcoord0), shadowProgress) * opacity;
        return;
    }
    vec4 color = texture2D(sampler, texcoord0);
    float effectiveRadius = min(radius, min(size.x, size.y) * 0.5);
    vec2 point = texcoord0 * size - size * 0.5;
    vec2 box = size * 0.5 - vec2(effectiveRadius);
    vec2 outside = max(abs(point) - box, vec2(0.0));
    float distance = length(outside) - effectiveRadius;
    float mask = effectiveRadius > 0.0 ? 1.0 - smoothstep(-0.5, 0.5, distance) : 1.0;
    gl_FragColor = color * (mask * opacity);
}

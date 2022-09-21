uniform vec3 iResolution;
uniform vec2 iOffset;
uniform vec4 geometryColor;
vec4 mix1(vec4 a, vec4 b, float t) {
    return a * (1.0 - t) + b * t; 
}
float smoothstep(float v0, float v1, float x) {
    float t;
    t = clamp((x - v0) / (v1 - v0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
void main()
{
    float bw1 = 4.0;  //border width

    float x = gl_FragCoord.x - iOffset.x;
    float y = gl_FragCoord.y - iOffset.y;

    float W = iResolution.x;
    float H = iResolution.y;

    float r = 8.6;
    float R = r;
    float Q = 4.0;
    vec4 G = geometryColor;
    vec4 U = vec4(G.xyz,0);

    if (x < R && y < R) {
        x -= 2.2; y -= 2.2;
        float d = distance(vec2(x,y), vec2(R,R));
        float q = r - d;
        float s = smoothstep(0.0f, Q, abs(q));
        vec4 C1 = mix1(G, U, s);
        gl_FragColor = C1;
    }
    else if (x < R && y > H - R) {   //left-top
        x -= 2.2; y += 2.2;
        float d = distance(vec2(x, y), vec2(R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix1(G, U, s);
        gl_FragColor = C1;
    }
    else if ( x > W - R && y > H - R) {
        x += 2.2; y += 2.2;
        float d = distance(vec2(x, y), vec2(W - R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix1(G, U, s);
        gl_FragColor = C1;
    }
    else if (x > W - R && y < R) {
        x += 2.0; y -= 2.0;
        float d = distance(vec2(x, y), vec2(W - R, R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix1(G, U, s);
        gl_FragColor = C1;
    }
    else if (x > -1.0 && x < bw1 && y > R && y < H - R)
        gl_FragColor = geometryColor;
    else if (x < W + 1.0 && x > W - bw1 && y > R && y < H - R)  //right
        gl_FragColor = geometryColor;
    else if (y > H - bw1 && y < H + 1.0 && x > R && x < W - R)
        gl_FragColor = geometryColor;
    else if (y < bw1 && y > -1.0 && x > R && x < W - R)
        gl_FragColor = geometryColor;
    else
        discard;

    gl_FragColor.a *= 2.8;
}

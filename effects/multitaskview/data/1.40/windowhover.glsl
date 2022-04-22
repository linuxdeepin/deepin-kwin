#version 140

uniform vec3 iResolution;
uniform vec2 iOffset;
uniform vec4 geometryColor;

out vec4 fragColor;

void main()
{
    float bw1 = 5.0f;  //border width

    float x = gl_FragCoord.x - iOffset.x;
    float y = gl_FragCoord.y - iOffset.y;

    float W = iResolution.x;
    float H = iResolution.y;

    float r = 18.0;
    float R = r;
    float Q = 4.4f;
    vec4 G = geometryColor;
    vec4 U = vec4(G.xyz,0);

    if (x < R && y < R) {
        x -= 2.2f; y -= 2.2f;
        float d = distance(vec2(x,y), vec2(R,R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix(G, U, s);
        fragColor = C1;
    }
    else if (x < R && y > H - R) {   //left-top
        x -= 2.2f; y += 2.2f;
        float d = distance(vec2(x, y), vec2(R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix(G, U, s);
        fragColor = C1;
    }
    else if ( x > W - R && y > H - R) {
        x += 2.2f; y += 2.2f;
        float d = distance(vec2(x, y), vec2(W - R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix(G, U, s);
        fragColor = C1;
    }
    else if (x > W - R && y < R) {
        x += 2.0f; y -= 2.0f;
        float d = distance(vec2(x, y), vec2(W - R, R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        vec4 C1 = mix(G, U, s);
        fragColor = C1;
    }
    else if (x > -1.0 && x < bw1 && y > R && y < H - R)
        fragColor = geometryColor;
    else if (x < W + 1.0 && x > W - bw1 && y > R && y < H - R)  //right
        fragColor = geometryColor;
    else if (y > H - bw1 && y < H + 1.0 && x > R && x < W - R)
        fragColor = geometryColor;
    else if (y < bw1 && y > -1.0 && x > R && x < W - R)
        fragColor = geometryColor;
    else
        discard;

    fragColor.a *= 2.8f;
}

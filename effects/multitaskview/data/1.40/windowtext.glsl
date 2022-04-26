#version 140

uniform vec3 iResolution;
uniform vec2 iOffset;

out vec4 fragColor;

void main()
{
    float bw1 = 1.0f;  //border width
    float x = gl_FragCoord.x - iOffset.x;
    float y = gl_FragCoord.y - iOffset.y;

    float W = iResolution.x;
    float H = iResolution.y;

    float r = 8.0;
    float R = r;
    float Q = 0.9f;
    vec4 G = vec4(0.0, 0.0, 0.0, 0.05);
    vec4 U = vec4(G.xyz, 0);
    vec4 U1 = vec4(0.97, 0.97, 0.97, 0.8);

    if (x < R && y < R) {
        float d = distance(vec2(x,y), vec2(R,R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        if (d < R) {
            vec4 C1 = mix(G, U1, s);
            fragColor = C1;
        }
        else {
            vec4 C1 = mix(G, U, s);
            fragColor = C1;
        }
    }
    else if (x < R && y > H - R) {   //left-top
        float d = distance(vec2(x, y), vec2(R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        float a = mix(0.0, 1.0, s);

        if (d < R) {
            vec4 C1 = mix(G, U1, s);
            fragColor = C1;
        }
        else {
            vec4 C1 = mix(G, U, s);
            fragColor = C1;
        }
    }
    else if ( x > W - R && y > H - R) {
        float d = distance(vec2(x, y), vec2(W - R, H - R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        if (d < R) {
            vec4 C1 = mix(G, U1, s);
            fragColor = C1;
        }
        else {
            vec4 C1 = mix(G, U, s);
            fragColor = C1;
        }
    }
    else if (x > W - R && y < R) {
        float d = distance(vec2(x, y), vec2(W - R, R));
        float q = r - d;
        float s = smoothstep(0.0, Q, abs(q));
        if (d < R) {
            vec4 C1 = mix(G, U1, s);
            fragColor = C1;
        } else {
            vec4 C1 = mix(G, U, s);
            fragColor = C1;
        }
    }
    else if (x > -1.0 && x < bw1 && y > R && y < H - R)
        fragColor = G;
    else if (x > bw1 && x < W - bw1 && y > R && y < H - R)
        fragColor = vec4(0.97, 0.97, 0.97, 0.8);
    else if (x < W + 1.0 && x > W - bw1 && y > R && y < H - R)  //right
        fragColor = G;
    else if (y > H - bw1 && y < H + 1.0 && x > R && x < W - R)
        fragColor = G;
    else if (y < bw1 && y > -1.0 && x > R && x < W - R)
        fragColor = G;
    else if (y > bw1 && y < H - bw1 && x > R && x < W - R)
        fragColor = vec4(0.97, 0.97, 0.97, 0.8);
    else
        discard;
}

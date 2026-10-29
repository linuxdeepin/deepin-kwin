#version 140
in vec2 position;
in vec2 texcoord;
uniform mat4 mvp;
uniform vec2 uSize;
uniform vec4 uSrcRect;
out vec2 vUV;
out vec2 vPos;
void main(void)
{
    vUV = uSrcRect.xy + texcoord * uSrcRect.zw;
    vPos = position * uSize;
    gl_Position = mvp * vec4(position, 0.0, 1.0);
}

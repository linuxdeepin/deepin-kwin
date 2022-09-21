attribute vec4 position;
attribute vec4 texcoord;
varying vec2 texcoord0;

uniform float dx;
uniform mat4 modelViewProjectionMatrix;

void main()
{
  texcoord0 = texcoord.st;
  vec4 pos = position;
  pos.x += dx;
  gl_Position = modelViewProjectionMatrix * pos;
}

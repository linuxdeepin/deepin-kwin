#version 140

in vec4 position;
in vec4 texcoord;
out vec2 texcoord0;

//uniform float dx;
uniform mat4 modelViewProjectionMatrix;

void main()
{
  texcoord0 = texcoord.st;
  gl_Position = modelViewProjectionMatrix * position;
}

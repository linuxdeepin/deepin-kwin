#version 140

uniform float dx;
uniform sampler2D sampler;
uniform vec2 iResolution;
uniform vec4 modulation;
uniform float saturation;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    float Pi = 6.28318530718; // Pi*2

    // GAUSSIAN BLUR SETTINGS {{{
    float Directions = 16.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    float Quality = 1.9; // BLUR QUALITY (Default 4.0 - More is better but slower)
    float Size = 4.0; // BLUR SIZE (Radius)
    // GAUSSIAN BLUR SETTINGS }}}

    vec2 Radius = Size/iResolution.xy;

    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = texcoord0;
    // Pixel colour
    vec4 Color = texture(sampler, uv);

    float calculateTimes = Pi/Directions;
    float qualities = 1.0/Quality;

    // Blur calculations
    for( float d=0.0; d<Pi; d+=calculateTimes)
    {
		for(float i=qualities; i<=1.0; i+=qualities)
        {
			Color += texture(sampler, uv+vec2(cos(d),sin(d))*Radius*i);
        }
    }

    // Output to screen
    Color /= Quality * Directions - 15.0;
    fragColor =  Color;
}

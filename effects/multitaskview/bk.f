#version 140

uniform float dx;
uniform sampler2D sampler;
//uniform vec3 iResolution;
uniform vec4 modulation;
uniform float saturation;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    float Pi = 6.283; // Pi*2

    // GAUSSIAN BLUR SETTINGS 
    float Directions = 16.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    float Quality = 4.0;     // BLUR QUALITY (Default 4.0 - More is better but slower)
    float Size = 12.0;        // BLUR SIZE (Radius)

   // vec2 Radius = Size/iResolution.xy;
    //vec2 Radius = Size/vec2(1920.0,1080.0);

    // Normalized pixel coordinates (from 0 to 1)
    //vec2 uv = fragCoord/iResolution.xy;
    vec2 uv = texcoord0;
    // Pixel colour
    vec4 Color = texture(sampler, uv);

 //   float D = Pi/Directions;
  //  float Q = 1.0/Quality;

    // Blur calculations
    //for( float d=0.0; d<Pi; d+=Pi/Directions)
//    for( float d=0.0; d<Pi; d+=D)
 //   {
  //    //for(float i=1.0/Quality; i<=1.0; i+=1.0/Quality)
   //   for(float i=Q; i<=1.0; i+=Q)
   //   {
   //         Color += texture( sampler, uv+vec2(cos(d),sin(d))*Radius*i);		
   //   }
//}


Color += texture(sampler, uv+vec2(0.001563, 0.000000));
Color += texture(sampler, uv+vec2(0.003125, 0.000000));
Color += texture(sampler, uv+vec2(0.004688, 0.000000));
Color += texture(sampler, uv+vec2(0.006250, 0.000000));
Color += texture(sampler, uv+vec2(0.001444, 0.001063));
Color += texture(sampler, uv+vec2(0.002887, 0.002126));
Color += texture(sampler, uv+vec2(0.004331, 0.003189));
Color += texture(sampler, uv+vec2(0.005774, 0.004252));
Color += texture(sampler, uv+vec2(0.001105, 0.001964));
Color += texture(sampler, uv+vec2(0.002210, 0.003928));
Color += texture(sampler, uv+vec2(0.003315, 0.005892));
Color += texture(sampler, uv+vec2(0.004420, 0.007857));
Color += texture(sampler, uv+vec2(0.000598, 0.002566));
Color += texture(sampler, uv+vec2(0.001196, 0.005133));
Color += texture(sampler, uv+vec2(0.001794, 0.007699));
Color += texture(sampler, uv+vec2(0.002392, 0.010265));
Color += texture(sampler, uv+vec2(0.000000, 0.002778));
Color += texture(sampler, uv+vec2(0.000000, 0.005556));
Color += texture(sampler, uv+vec2(0.000000, 0.008333));
Color += texture(sampler, uv+vec2(0.000000, 0.011111));
Color += texture(sampler, uv+vec2(-0.000598, 0.002566));
Color += texture(sampler, uv+vec2(-0.001196, 0.005133));
Color += texture(sampler, uv+vec2(-0.001794, 0.007699));
Color += texture(sampler, uv+vec2(-0.002391, 0.010266));
Color += texture(sampler, uv+vec2(-0.001105, 0.001964));
Color += texture(sampler, uv+vec2(-0.002210, 0.003929));
Color += texture(sampler, uv+vec2(-0.003314, 0.005893));
Color += texture(sampler, uv+vec2(-0.004419, 0.007857));
Color += texture(sampler, uv+vec2(-0.001444, 0.001063));
Color += texture(sampler, uv+vec2(-0.002887, 0.002126));
Color += texture(sampler, uv+vec2(-0.004331, 0.003190));
Color += texture(sampler, uv+vec2(-0.005774, 0.004253));
Color += texture(sampler, uv+vec2(-0.001563, 0.000000));
Color += texture(sampler, uv+vec2(-0.003125, 0.000001));
Color += texture(sampler, uv+vec2(-0.004688, 0.000001));
Color += texture(sampler, uv+vec2(-0.006250, 0.000001));
Color += texture(sampler, uv+vec2(-0.001444, -0.001063));
Color += texture(sampler, uv+vec2(-0.002887, -0.002125));
Color += texture(sampler, uv+vec2(-0.004331, -0.003188));
Color += texture(sampler, uv+vec2(-0.005774, -0.004251));
Color += texture(sampler, uv+vec2(-0.001105, -0.001964));
Color += texture(sampler, uv+vec2(-0.002210, -0.003928));
Color += texture(sampler, uv+vec2(-0.003315, -0.005892));
Color += texture(sampler, uv+vec2(-0.004420, -0.007856));
Color += texture(sampler, uv+vec2(-0.000598, -0.002566));
Color += texture(sampler, uv+vec2(-0.001196, -0.005132));
Color += texture(sampler, uv+vec2(-0.001794, -0.007699));
Color += texture(sampler, uv+vec2(-0.002393, -0.010265));
Color += texture(sampler, uv+vec2(-0.000000, -0.002778));
Color += texture(sampler, uv+vec2(-0.000000, -0.005556));
Color += texture(sampler, uv+vec2(-0.000001, -0.008333));
Color += texture(sampler, uv+vec2(-0.000001, -0.011111));
Color += texture(sampler, uv+vec2(0.000598, -0.002566));
Color += texture(sampler, uv+vec2(0.001195, -0.005133));
Color += texture(sampler, uv+vec2(0.001793, -0.007699));
Color += texture(sampler, uv+vec2(0.002391, -0.010266));
Color += texture(sampler, uv+vec2(0.001105, -0.001965));
Color += texture(sampler, uv+vec2(0.002209, -0.003929));
Color += texture(sampler, uv+vec2(0.003314, -0.005894));
Color += texture(sampler, uv+vec2(0.004419, -0.007858));
Color += texture(sampler, uv+vec2(0.001443, -0.001063));
Color += texture(sampler, uv+vec2(0.002887, -0.002127));
Color += texture(sampler, uv+vec2(0.004330, -0.003190));
Color += texture(sampler, uv+vec2(0.005774, -0.004254));
Color += texture(sampler, uv+vec2(0.001562, -0.000001));
Color += texture(sampler, uv+vec2(0.003125, -0.000001));
Color += texture(sampler, uv+vec2(0.004687, -0.000002));
Color += texture(sampler, uv+vec2(0.006250, -0.000002));


    // Output to screen
    Color /= Quality * Directions - 15.0;
    fragColor =  Color*0.78;
    //fragColor = vec4(0.6,0.6,0.2,1.0);
}

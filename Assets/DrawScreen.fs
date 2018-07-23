#version 330 core
out vec4 FragColor;
  
in vec2 TexCoords;

uniform int doBlur;
uniform sampler2D lastScreen; 
uniform sampler2D blurredScreen; 

void main()
{ 
  float bluramt = doBlur == 1 ? 0.6 : 0;
  FragColor = mix(texture(lastScreen, TexCoords), texture(blurredScreen, TexCoords), bluramt);
}


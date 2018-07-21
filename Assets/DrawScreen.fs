#version 330 core
out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D lastScreen; 
uniform sampler2D blurredScreen; 

void main()
{ 
  FragColor = mix(texture(lastScreen, TexCoords), texture(blurredScreen, TexCoords), 0.5);
}


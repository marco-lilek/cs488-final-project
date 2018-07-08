#version 330 core
layout (location = 0) in vec3 position;

uniform mat4 LightSpaceMatrix;
uniform mat4 Model;

void main()
{
    gl_Position = LightSpaceMatrix * Model * vec4(position, 1.0);
}  

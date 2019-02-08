#version 330 core
layout (location = 0) in vec3 aPos;

out VsOutFsIn {
  out vec3 TexCoords;

} vs_out;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;

void main()
{
    vs_out.TexCoords = aPos;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}  

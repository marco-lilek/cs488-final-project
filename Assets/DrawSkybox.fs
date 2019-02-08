#version 330
out vec4 FragColor;

in VsOutFsIn {
  in vec3 TexCoords;
} fs_in;

uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, fs_in.TexCoords);
}

#version 330

// Model-Space coordinates
in vec3 position;
in vec3 normal;
in vec2 vertexUV;
in vec3 aTangent;

struct LightSource {
    vec3 position;
    vec3 rgbIntensity;
};
uniform LightSource light;
uniform vec3 viewPos;

uniform mat4 LightSpaceMatrix;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Perspective;

// Remember, this is transpose(inverse(ModelView)).  Normals should be
// transformed using this matrix instead of the ModelView matrix.
uniform mat3 NormalMatrix;

out VsOutFsIn {
  vec2 texCoords;
  vec4 fragPosLightSpace;
	LightSource light;

  vec3 TangentLightPos;
  vec3 TangentViewPos;
  vec3 TangentFragPos;

} vs_out;

void main() {
	vec4 pos4 = vec4(position, 1.0);

  vs_out.texCoords = vertexUV;
  vs_out.fragPosLightSpace = LightSpaceMatrix * Model * pos4;

	vs_out.light = light;

  vec3 T = normalize(NormalMatrix * aTangent);
  vec3 N = normalize(NormalMatrix * normal);

  T = normalize(T-dot(T,N) * N);
  vec3 B = cross(N,T);

  mat3 TBN = transpose(mat3(T,B,N));

  vs_out.TangentLightPos = TBN * light.position;
  vs_out.TangentViewPos = TBN * viewPos;
  vs_out.TangentFragPos = TBN * vec3(Model * pos4);
  

	gl_Position = Perspective * View * Model * pos4;
}

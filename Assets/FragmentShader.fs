#version 330

uniform sampler2D textureSampler;
uniform sampler2D normalMap;
uniform sampler2D shadowMap;

struct LightSource {
    vec3 position;
    vec3 rgbIntensity;
};

in VsOutFsIn {
	vec3 position_ES; // Eye-space position
	vec3 normal_ES;   // Eye-space normal
  vec3 lightSource_ES;
  vec2 texCoords;
  vec4 fragPosLightSpace;
	LightSource light;

  vec3 TangentLightPos;
  vec3 TangentViewPos;
  vec3 TangentFragPos;
} fs_in;


out vec4 fragColour;

struct Material {
    vec3 kd;
    vec3 ks;
    float shininess;
};
uniform Material material;

// Ambient light intensity for each RGB component.
uniform vec3 ambientIntensity;

vec2 poissondist[4] = vec2[](
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);
vec3 phongModel(vec4 fragPosLightSpace, vec3 fragPosition, vec3 fragNormal) {
	LightSource light = fs_in.light;

  // Direction from fragment to light source.
  vec3 l = normalize(fs_in.lightSource_ES - fragPosition);

  // Direction from fragment to viewer (origin - fragPosition).
  vec3 v = normalize(-fragPosition.xyz);

  float n_dot_l = max(dot(fragNormal, l), 0.0);

	vec3 diffuse;
	diffuse = material.kd * n_dot_l;

  vec3 specular = vec3(0.0);

  if (n_dot_l > 0.0) {
  // Halfway vector.
  vec3 h = normalize(v + l);
      float n_dot_h = max(dot(fragNormal, h), 0.0);

      specular = material.ks * pow(n_dot_h, material.shininess);
  }

  vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
  projCoords = projCoords * 0.5 + 0.5;
  
  float biasMAX = 0.05;
  float bias = clamp(biasMAX*tan(acos(clamp(n_dot_l,0,1))),0,biasMAX);
  float currentDepth = projCoords.z;

  float shadow = 1; 
  for (int i = 0; i < 4; i++) {
    float closestDepth = texture(shadowMap, projCoords.xy + poissondist[i]/700,0).r;

    if (currentDepth - bias < closestDepth) {
      shadow -= 0.25;
    }
  }

  if (projCoords.z > 1.0)
    shadow = 0.0;

  return (ambientIntensity + (1-shadow)* (diffuse + specular)) * light.rgbIntensity;
}

void main() {
  vec3 textureSample = texture(textureSampler, fs_in.texCoords).rgb;
  vec3 phongSample = phongModel(fs_in.fragPosLightSpace, fs_in.position_ES, fs_in.normal_ES);
  if (phongSample.x < 0) fragColour = vec4(textureSample,1.0);
  fragColour = vec4(phongSample*textureSample, 1.0);
}

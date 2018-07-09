#version 330

uniform sampler2D textureSampler;
uniform sampler2D normalMap;
uniform sampler2D shadowMap;

struct LightSource {
    vec3 position;
    vec3 rgbIntensity;
};

in VsOutFsIn {
  vec2 texCoords;
  vec4 fragPosLightSpace;
	LightSource light;

  vec3 TangentLightPos;
  vec3 TangentViewPos;
  vec3 TangentFragPos;

} fs_in;


layout(location=0) out vec4 fragColour;

struct Material {
    vec3 kd;
    vec3 ks;
    float shininess;
    float transparency;
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

void main() {

  vec3 normal;
  normal = texture(normalMap, fs_in.texCoords).rgb;
  normal = normalize(normal*2.0 - 1.0);
  
  vec3 color = texture(textureSampler, fs_in.texCoords).rgb;
  vec3 ambient = ambientIntensity * color;
 
  vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
  vec3 diffuse = max(dot(lightDir, normal),0.0) * material.kd * color;

  vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
  vec3 halfwayDir = normalize(lightDir + viewDir);
  vec3 specular = material.ks * pow(max(dot(normal, halfwayDir),0.0),material.shininess);

  // shadows
  vec3 projCoords = fs_in.fragPosLightSpace.xyz / fs_in.fragPosLightSpace.w;
  projCoords = projCoords * 0.5 + 0.5;

  float biasMAX = 0.01;
  float bias = 0.01; //clamp(biasMAX*tan(acos(clamp(dot(normal, lightDir),0,1))),0,biasMAX);
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


  fragColour = vec4(ambient + (1-shadow)*(diffuse + specular), material.transparency);
}

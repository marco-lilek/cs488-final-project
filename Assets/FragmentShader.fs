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
  vec3 correctNormal;
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
uniform int showShadows;
uniform int showTextures;

vec2 poissondist[4] = vec2[](
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);

void main() {

  vec3 normal;
  normal = texture(normalMap, fs_in.texCoords).rgb;
  if (normal.x == 0 && normal.y == 0 && normal.z == 0)
    //normal = fs_in.correctNormal;
    normal = vec3(0,0,1);
  else
    normal = normalize(normal*2.0 - 1.0);

  vec3 color = texture(textureSampler, fs_in.texCoords).rgb;

  if (showTextures == 0) color = vec3(1);

  vec3 ambient = ambientIntensity * color;
 
  vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
  vec3 diffuse = max(dot(lightDir, normal),0.0) * material.kd * color;

  vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
  vec3 halfwayDir = normalize(lightDir + viewDir);
  vec3 specular = material.ks * pow(max(dot(normal, halfwayDir),0.0),material.shininess);

  // shadows
  vec3 projCoords = fs_in.fragPosLightSpace.xyz / fs_in.fragPosLightSpace.w;
  projCoords = projCoords * 0.5 + 0.5;

  float biasMAX = 0.0001;
  float bias = 0.0005* tan(acos(clamp(dot(normal, lightDir),0,1)));
  bias = clamp(bias,0,biasMAX);
  float currentDepth = projCoords.z;

  float shadow = 0; 
  for (int i = 0; i < 4; i++) {
    float closestDepth = texture(shadowMap, projCoords.xy + poissondist[i]/1000,0).r;

    if (currentDepth - bias > closestDepth) {
      shadow += 0.25;
    }
  }

  if (projCoords.z > 1.0) // TODO: shadow mapping
    shadow = 0.0;

  shadow = shadow * showShadows;

  fragColour = vec4(ambient + (1-shadow)*(diffuse + specular),material.transparency);
}

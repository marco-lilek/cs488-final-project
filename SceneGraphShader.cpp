#include "SceneGraphShader.hpp"
#include "cs488-framework/GlErrorCheck.hpp"
#include "Project.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "GeometryNode.hpp"
#include <iostream>

using namespace std;
using namespace glm;

SceneGraphShader::SceneGraphShader() {
  glGenVertexArrays(1, &m_vao);
}

void DepthMapShader::enableVertexShaderInputSlots() {
  glBindVertexArray(m_vao);
  // Enable the vertex shader attribute location for "position" when rendering.
  m_positionAttribLocation = getAttribLocation("position");
  glEnableVertexAttribArray(m_positionAttribLocation);
  
  CHECK_GL_ERRORS;
  glBindVertexArray(0);
}

void DepthMapShader::mapVboDataToVertexShaderInputs(Project *project) {
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, project->m_vbo_vertexPositions);
  glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  //-- Unbind target, and restore default values:
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  CHECK_GL_ERRORS;
}

void DepthMapShader::uploadCommonSceneUniforms(Project *project) {
  enable();
  GLint location = getUniformLocation("LightSpaceMatrix");
  glUniformMatrix4fv(location,1,GL_FALSE, value_ptr(project->m_lightSpaceMatrix));
  CHECK_GL_ERRORS;
  disable();
}

void DepthMapShader::updateShaderUniforms(Project *project,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix) const {
  enable();
  GLint location = getUniformLocation("Model");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(nodeTrans));
  CHECK_GL_ERRORS;
  disable();
}

void SceneShader::enableVertexShaderInputSlots() {
  glBindVertexArray(m_vao);

  // Enable the vertex shader attribute location for "position" when rendering.
  m_positionAttribLocation = getAttribLocation("position");
  glEnableVertexAttribArray(m_positionAttribLocation);
  
  CHECK_GL_ERRORS;

  // Enable the vertex shader attribute location for "normal" when rendering.
  m_normalAttribLocation = getAttribLocation("normal");
  glEnableVertexAttribArray(m_normalAttribLocation);

  CHECK_GL_ERRORS;
  
  // Enable the vertex shader attribute location for "uv" when rendering.
  m_uvAttribLocation = getAttribLocation("vertexUV");
  glEnableVertexAttribArray(m_uvAttribLocation);

  CHECK_GL_ERRORS;

  // Enable the vertex shader attribute location for "uv" when rendering.
  m_tangentAttribLocation = getAttribLocation("aTangent");
  glEnableVertexAttribArray(m_tangentAttribLocation);

  CHECK_GL_ERRORS;
  glBindVertexArray(0);
}

void SceneShader::mapVboDataToVertexShaderInputs(Project *project) {
  // Bind VAO in order to record the data mapping.
  glBindVertexArray(m_vao);

  CHECK_GL_ERRORS;
  // Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
  // "position" vertex attribute location for any bound vertex shader program.
  glBindBuffer(GL_ARRAY_BUFFER, project->m_vbo_vertexPositions);
  CHECK_GL_ERRORS;
  glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  CHECK_GL_ERRORS;
  // Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
  // "normal" vertex attribute location for any bound vertex shader program.
  glBindBuffer(GL_ARRAY_BUFFER, project->m_vbo_vertexNormals);
  glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  CHECK_GL_ERRORS;
  // Tell GL how to map data from the vertex buffer "m_vbo_vertexUVs" into the
  // "uv" vertex attribute location for any bound vertex shader program.
  glBindBuffer(GL_ARRAY_BUFFER, project->m_vbo_vertexUVs);
  glVertexAttribPointer(m_uvAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  CHECK_GL_ERRORS;
  glBindBuffer(GL_ARRAY_BUFFER, project->m_vbo_vertexTangents);
  glVertexAttribPointer(m_tangentAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  CHECK_GL_ERRORS;
  //-- Unbind target, and restore default values:
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  CHECK_GL_ERRORS;
}

void SceneShader::uploadCommonSceneUniforms(Project *project) {
  enable();
  //-- Set Perpsective matrix uniform for the scene:
  GLint location = getUniformLocation("Perspective");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(project->m_perpsective));
  CHECK_GL_ERRORS;
  
  location = getUniformLocation("showShadows");
  glUniform1i(location, project->m_show_shadows);

  {
    location = getUniformLocation("viewPos");
    glUniform3fv(location, 1, value_ptr(project->m_viewPos));
  }

  //-- Set LightSource uniform for the scene:
  {
    location = getUniformLocation("light.position");
    glUniform3fv(location, 1, value_ptr(project->m_light.position));
    location = getUniformLocation("light.rgbIntensity");
    glUniform3fv(location, 1, value_ptr(project->m_light.rgbIntensity));
    CHECK_GL_ERRORS;
  }

  //-- Set background light ambient intensity
  {
    location = getUniformLocation("ambientIntensity");
    vec3 ambientIntensity(0.1f);
    glUniform3fv(location, 1, value_ptr(ambientIntensity));
    CHECK_GL_ERRORS;
  }

  //-- 
  {
    location = getUniformLocation("LightSpaceMatrix");
    glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(project->m_lightSpaceMatrix));
    CHECK_GL_ERRORS;
  }

  disable();
}
void SceneShader::updateShaderUniforms(Project *project,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix) const {
  enable();
  //-- Set ModelView matrix:
  GLint location = getUniformLocation("View");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(viewMatrix));
  CHECK_GL_ERRORS;

  location = getUniformLocation("Model");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(nodeTrans));
  CHECK_GL_ERRORS;


  //-- Set Material values:
  location = getUniformLocation("material.kd");
  vec3 kd = node->material.kd;
  glUniform3fv(location, 1, value_ptr(kd));
  CHECK_GL_ERRORS;
  location = getUniformLocation("material.ks");
  vec3 ks = node->material.ks;
  glUniform3fv(location, 1, value_ptr(ks));
  CHECK_GL_ERRORS;
  location = getUniformLocation("material.shininess");
  glUniform1f(location, node->material.shininess);
  CHECK_GL_ERRORS;
  location = getUniformLocation("material.transparency");
  glUniform1f(location, (!project->m_show_transparent) ? 1.0 : node->material.transparency);
  CHECK_GL_ERRORS;
  location = getUniformLocation("showTextures");
  glUniform1i(location, project->m_show_textures);

  disable();

  glActiveTexture(GL_TEXTURE0);
  if (!project->m_show_textures) {
    glBindTexture(GL_TEXTURE_2D, 0);
  } else if (node->texture == "~perlin") {
    glBindTexture(GL_TEXTURE_2D, project->m_noiseTexture);
  } else {
    glBindTexture(GL_TEXTURE_2D, project->m_textureNameIdMap[node->texture]);
  }

  glActiveTexture(GL_TEXTURE1);
  if (!project->m_show_bump) {
    glBindTexture(GL_TEXTURE_2D, 0);
  } else {
    glBindTexture(GL_TEXTURE_2D, project->m_bumpNameIdMap[node->bumpMap]);
  }
}

void SceneShader::setTextureMaps() {
  enable();
  GLuint location = getUniformLocation("textureSampler");
  glUniform1i(location, 0);
  location = getUniformLocation("normalMap");
  glUniform1i(location, 1);
  location = getUniformLocation("shadowMap");
  glUniform1i(location, 2);
  disable();
}

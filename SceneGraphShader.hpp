#pragma once

#include "cs488-framework/ShaderProgram.hpp"
#include "GeometryNode.hpp"
#include <glm/gtc/type_ptr.hpp>

class Project;

class SceneGraphShader : public ShaderProgram {
protected:
  GLuint m_positionAttribLocation;
  GLuint m_normalAttribLocation;
  GLuint m_uvAttribLocation;
  GLuint m_tangentAttribLocation;
public:
  GLuint m_vao;
  
  SceneGraphShader();
  virtual void enableVertexShaderInputSlots() {};
  virtual void setTextureMaps() {}
  virtual void mapVboDataToVertexShaderInputs(Project *project) {};
  virtual void uploadCommonSceneUniforms(Project *project) {};
  virtual void updateShaderUniforms(Project *project,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix) const = 0;
};

class DepthMapShader : public SceneGraphShader {
  virtual void enableVertexShaderInputSlots();
  virtual void mapVboDataToVertexShaderInputs(Project *project);
  virtual void uploadCommonSceneUniforms(Project *project);
  virtual void updateShaderUniforms(Project *project,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix) const;
};

class SceneShader : public SceneGraphShader {
  virtual void enableVertexShaderInputSlots();
  virtual void setTextureMaps();
  virtual void mapVboDataToVertexShaderInputs(Project *project);
  virtual void uploadCommonSceneUniforms(Project *project);
  virtual void updateShaderUniforms(Project *project,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix) const;
};

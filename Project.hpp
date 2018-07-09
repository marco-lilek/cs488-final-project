#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SceneNode.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <map>

struct LightSource {
	glm::vec3 position;
	glm::vec3 rgbIntensity;
};


class GeometryNode;
class Project : public CS488Window {
public:
	Project(const std::string & luaSceneFile);
	virtual ~Project();

protected:
	virtual void init() override;
	virtual void appLogic() override;
	virtual void guiLogic() override;
	virtual void draw() override;
	virtual void cleanup() override;

	//-- Virtual callback methods
	virtual bool cursorEnterWindowEvent(int entered) override;
	virtual bool mouseMoveEvent(double xPos, double yPos) override;
	virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
	virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
	virtual bool windowResizeEvent(int width, int height) override;
	virtual bool keyInputEvent(int key, int action, int mods) override;

	//-- One time initialization methods:
	void processLuaSceneFile(const std::string & filename);
	void createShaderProgram();
	void enableVertexShaderInputSlots();
	void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
	void mapVboDataToVertexShaderInputLocations();
	void initViewMatrix();
	void initLightSources();

	void initPerspectiveMatrix();
	void uploadCommonSceneUniforms();
void updateShaderUniforms(
		const ShaderProgram & shader,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix
);
	void renderSceneGraph(const ShaderProgram &shader,const SceneNode &node);
  void renderSceneGraphRecursive(const ShaderProgram &shader, const glm::mat4 &parentTransform, const SceneNode &root);
	void renderArcCircle();

	glm::mat4 m_perpsective;
	glm::mat4 m_view;
  glm::vec3 m_viewPos;

	LightSource m_light;

	//-- GL resources for mesh geometry data:
	GLuint m_vao_meshData;
	GLuint m_vbo_vertexPositions;
	GLuint m_vbo_vertexNormals;
	GLuint m_vbo_vertexUVs;
	GLint m_positionAttribLocation;
	GLint m_normalAttribLocation;
	GLint m_uvAttribLocation;
	ShaderProgram m_shader;

  std::map<std::string, GLuint> m_textureNameIdMap;
  void loadTextures();
  GLuint *m_textures;

  GLuint m_fbo_depthMap;
  GLuint m_vao_depthData;
  GLuint m_depthPositionAttribLocation;
  GLuint m_depthMap;
  void initDepthMapFBO();
  void createDepthMapShaderProgram();
  ShaderProgram m_depthMapShader;

  GLuint m_shaderID;
  GLuint m_depthMapShaderID;

  GLuint m_noiseTexture;
  void loadNoiseTexture();

  glm::mat4 m_lightSpaceMatrix;

  GLuint m_vbo_vertexTangents;
  GLuint m_tangentAttribLocation;

  GLuint m_normalMap;

	// BatchInfoMap is an associative container that maps a unique MeshId to a BatchInfo
	// object. Each BatchInfo object contains an index offset and the number of indices
	// required to render the mesh with identifier MeshId.
	BatchInfoMap m_batchInfoMap;

	std::string m_luaSceneFile;

	std::shared_ptr<SceneNode> m_rootNode;
  void collectTransparentNodesRecursive(const SceneNode &root, const glm::mat4 &parentTransform, std::vector<std::pair<const GeometryNode *, glm::mat4> > &transparentNodes);
};

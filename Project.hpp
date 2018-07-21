#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SoundManager.hpp"
#include "SceneNode.hpp"
#include "SceneGraphShader.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <map>
#include <string>
#include <set>

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
  void enableVertexShaderInputSlots();
  void setTextureMaps();
  void initWindowFBO(GLuint *fbo, GLuint *tex);
	void mapVboDataToVertexShaderInputLocations();
  void createShader(ShaderProgram &program, const std::string &vs, const std::string &fs);

	void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
	void initViewMatrix();
	void initLightSources();

	void initPerspectiveMatrix();
	void uploadCommonSceneUniforms();
	void renderSceneGraph(const SceneGraphShader &shader,const SceneNode &node, bool renderTransparent = false);
  void renderSceneGraphRecursive(const SceneGraphShader &shader, const glm::mat4 &parentTransform, const SceneNode &root, bool renderTransparent);

  void renderTransparentNodes(const SceneGraphShader &shader, const SceneNode &root);
	void renderArcCircle();


  void loadTextures();
  void loadTexture(const std::string &texture, char *defaultData);
  void loadTexturesFromList(std::set<std::string> &textures, std::map<std::string, GLuint> &nameMap, GLuint *texs, char *defaultData);

public:
  
  std::map<std::string, GLuint> m_bumpNameIdMap;
  GLuint *m_bumps;

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
	SceneGraphShader *m_shader;

  std::map<std::string, GLuint> m_textureNameIdMap;
  GLuint *m_textures;

  GLuint m_fbo_depthMap;
  GLuint m_vao_depthData;
  GLuint m_depthPositionAttribLocation;
  GLuint m_depthMap;
  void initDepthMapFBO();
  SceneGraphShader *m_depthMapShader;

  GLuint m_noiseTexture;
  void loadNoiseTexture();

  glm::mat4 m_lightSpaceMatrix;

  GLuint m_vbo_vertexTangents;
  GLuint m_tangentAttribLocation;

  GLuint m_normalMap;

  GLuint m_fbo_blurredScreen;
  GLuint m_blurredScreen;
  GLuint m_fbo_lastScreen;
  GLuint m_lastScreen;
  GLuint m_vao_screen;
  
  ShaderProgram m_screenShader;
  GLuint m_aPosAttribLocation;
  GLuint m_aTexCoordsAttribLocation;
  GLuint m_vbo_screenData;
  
  void initBlurFBO();

	// BatchInfoMap is an associative container that maps a unique MeshId to a BatchInfo
	// object. Each BatchInfo object contains an index offset and the number of indices
	// required to render the mesh with identifier MeshId.
	BatchInfoMap m_batchInfoMap;

	std::string m_luaSceneFile;

	std::shared_ptr<SceneNode> m_rootNode;
  void collectTransparentNodesRecursive(const SceneNode &root, const glm::mat4 &parentTransform, std::vector<std::pair<const GeometryNode *, glm::mat4> > &transparentNodes);

  void hookControls(std::vector<GeometryNode*> &nodes);
  GeometryNode * m_playerNode;

  void tickGameLogic();
  void tickBubbleMovement();
  void initGameLogic();

  void checkBBoxCollisions();
  void checkBBlCollisions();

  std::set<GeometryNode *> m_fixedBubbles;

  SoundManager m_soundManager;
  friend class SceneGraphShader;
};

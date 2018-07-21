#include "debug.hpp"
#include "Project.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"
#include "PerlinNoise.hpp"
#include "stb_image.h"
#include <algorithm>

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <set>
#include <string>

using namespace glm;

// NOTE: x is inverted

static bool show_gui = true;

const size_t CIRCLE_PTS = 48;
const size_t SHADOW_DIM = 1024;
const size_t SHADOW_WIDTH = SHADOW_DIM, SHADOW_HEIGHT = SHADOW_DIM;

//----------------------------------------------------------------------------------------
// Constructor
Project::Project(const std::string & luaSceneFile)
	: m_luaSceneFile(luaSceneFile),
	  m_positionAttribLocation(0),
	  m_normalAttribLocation(0),
	  m_uvAttribLocation(0),
	  m_tangentAttribLocation(0),
	  m_vao_meshData(0),
    m_vao_screen(0),
	  m_vbo_vertexPositions(0),
	  m_vbo_vertexNormals(0),
	  m_vbo_vertexUVs(0),
	  m_vbo_vertexTangents(0)
{
  m_viewPos = vec3(-0.6,5,-10);
}

//----------------------------------------------------------------------------------------
// Destructor
Project::~Project()
{
  delete[] m_textures;
  // TODO
  delete m_shader;
  delete m_depthMapShader;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void Project::init()
{
	// Set the background colour.
	glClearColor(0.35, 0.35, 0.35, 1.0);

  m_shader = new SceneShader();
  m_depthMapShader = new DepthMapShader();
  createShader(*m_shader, "VertexShader.vs", "FragmentShader.fs");
  createShader(*m_depthMapShader, "DepthMapVertexShader.vs", "DepthMapFragmentShader.fs");
  createShader(m_screenShader, "DrawScreen.vs", "DrawScreen.fs");

	glGenVertexArrays(1, &m_vao_meshData);
	glGenVertexArrays(1, &m_vao_screen);
	glGenVertexArrays(1, &m_vao_depthData);

  enableVertexShaderInputSlots();

	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
			getAssetFilePath("cube.obj"),
			getAssetFilePath("sphere.obj")
	});


	// Acquire the BatchInfoMap from the MeshConsolidator.
	meshConsolidator->getBatchInfoMap(m_batchInfoMap);

	// Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
	uploadVertexDataToVbos(*meshConsolidator);

  CHECK_GL_ERRORS;
  mapVboDataToVertexShaderInputLocations();

  setTextureMaps();

  initBlurFBO();
  initDepthMapFBO();


	initPerspectiveMatrix();

	initLightSources();
  initViewMatrix();


  loadTextures();
  loadNoiseTexture();

  initGameLogic();
}

void Project::initWindowFBO(GLuint *fbo, GLuint *tex) {
  glGenFramebuffers(1,fbo);
  glGenTextures(1,tex);

  glBindFramebuffer(GL_FRAMEBUFFER, *fbo);

  glBindTexture(GL_TEXTURE_2D, *tex);

  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, m_windowWidth, m_windowHeight, 0,GL_RGBA, GL_UNSIGNED_BYTE, 0);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  CHECK_GL_ERRORS;
  
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *tex, 0);
  CHECK_GL_ERRORS;

  // Set the list of draw buffers.
  GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
  CHECK_GL_ERRORS;

}

void Project::initBlurFBO() {
  initWindowFBO(&m_fbo_blurredScreen, &m_blurredScreen);
  {
  CHECK_GL_ERRORS;
  GLuint depthrenderbuffer;
  CHECK_GL_ERRORS;
  glGenRenderbuffers(1, &depthrenderbuffer);
  CHECK_GL_ERRORS;
  glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_windowWidth, m_windowHeight);

  initWindowFBO(&m_fbo_lastScreen, &m_lastScreen);

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer); 
  
  CHECK_GL_ERRORS;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER,0);
  glBindTexture(GL_TEXTURE_2D,0);
  }
}

void Project::initDepthMapFBO() {
  glGenFramebuffers(1, &m_fbo_depthMap);

  glGenTextures(1, &m_depthMap);
  glBindTexture(GL_TEXTURE_2D, m_depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
             SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);  
  float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D,0);
  CHECK_GL_ERRORS;
}

void Project::loadNoiseTexture() {
  glGenTextures(1, &m_noiseTexture);
  
  glBindTexture(GL_TEXTURE_2D, m_noiseTexture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  PerlinNoise noise;
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, noise.NOISE_DIM, noise.NOISE_DIM, 0, GL_RGB, GL_UNSIGNED_BYTE, noise.getNoise().get());
  CHECK_GL_ERRORS;
}

void Project::loadTexture(const string &texture, char *defaultData) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (texture.empty()) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, defaultData);
    DEBUGM(cerr << "loaded default texture" << endl);
  } else {
    int width, height, nrChannels;
    const string texPath = getAssetFilePath(texture.c_str());
    unsigned char *data = stbi_load(texPath.c_str(), &width, &height, &nrChannels, 0);
    DEBUGM(cerr << "loaded " << texPath << endl);
    if (data) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      //glGenerateMipmap(GL_TEXTURE_2D);
    } else {
      std::cout << "Failed to load " << texPath << std::endl;
      assert(0);
    }
  }
}

void Project::loadTexturesFromList(set<string> &textures, map<string, GLuint> &nameMap, GLuint *texs, char *defaultData) {
  DEBUGM(cerr << "generating " << textures.size() << " textures" << endl);
  texs = new GLuint[textures.size()];
  glGenTextures(textures.size(), texs);
  int texId = 0;
  for (set<string>::iterator it = textures.begin(); it != textures.end(); it++, texId++) {
    const string &texture = *it;
    if (texture[0] == '~') continue;
    glBindTexture(GL_TEXTURE_2D, texs[texId]);
    loadTexture(texture, defaultData);
    nameMap[texture] = texs[texId];

    CHECK_GL_ERRORS; // we might run out of space
    glBindTexture(GL_TEXTURE_2D,0);
  }
}

void Project::loadTextures() {
  vector<GeometryNode *> nodes;
  m_rootNode->getGeometryNodes(nodes);

  set<string> textures;
  set<string> bumpMaps;
  for (vector<GeometryNode *>::iterator it = nodes.begin(); it != nodes.end(); it++) {
    GeometryNode *node = *it;
    textures.insert(node->texture);
    bumpMaps.insert(node->bumpMap);
  }
  
  unsigned char defaultTex[] = {255,255,255};
  loadTexturesFromList(textures, m_textureNameIdMap, m_textures, (char*)defaultTex);

  unsigned char defaultBump[] = {0,0,0};
  loadTexturesFromList(bumpMaps, m_bumpNameIdMap, m_bumps, (char*)defaultBump);
}

//----------------------------------------------------------------------------------------
void Project::processLuaSceneFile(const std::string & filename) {
	// This version of the code treats the Lua file as an Asset,
	// so that you'd launch the program with just the filename
	// of a puppet in the Assets/ directory.
	// std::string assetFilePath = getAssetFilePath(filename.c_str());
	// m_rootNode = std::shared_ptr<SceneNode>(import_lua(assetFilePath));

	// This version of the code treats the main program argument
	// as a straightforward pathname.
	m_rootNode = std::shared_ptr<SceneNode>(import_lua(filename));
	if (!m_rootNode) {
		std::cerr << "Could not open " << filename << std::endl;
	}
  
  vector<GeometryNode *> nodes;
  m_rootNode->getGeometryNodes(nodes);
  hookControls(nodes);
}

void Project::hookControls(vector<GeometryNode*> &nodes) {
  for (vector<GeometryNode *>::iterator it = nodes.begin(); it != nodes.end(); it++) {
    GeometryNode *node = *it;
    if (node->m_name == "~player") {
      m_playerNode = node;
    } else if (node->meshId == "sphere") {
      m_fixedBubbles.insert(node);
    }
  }
}

void Project::initGameLogic() {
  m_playerNode->setPos(vec3(0,2,0));
  m_playerNode->setMoveVector(vec3(0.1f,0.05f,0));
}

void Project::createShader(ShaderProgram &program, const string &vs, const string &fs) {
	program.generateProgramObject();
	program.attachVertexShader( getAssetFilePath(vs.c_str()).c_str() );
	program.attachFragmentShader( getAssetFilePath(fs.c_str()).c_str() );
	program.link();
}


//----------------------------------------------------------------------------------------
void Project::enableVertexShaderInputSlots()
{
  m_shader->enableVertexShaderInputSlots();
  m_depthMapShader->enableVertexShaderInputSlots();
	//-- Enable input slots for m_vao_meshData:
  {
    glBindVertexArray(m_vao_screen);
    m_aPosAttribLocation = m_screenShader.getAttribLocation("aPos");
    glEnableVertexAttribArray(m_aPosAttribLocation);
    CHECK_GL_ERRORS;

    m_aTexCoordsAttribLocation = m_screenShader.getAttribLocation("aTexCoords");
    glEnableVertexAttribArray(m_aTexCoordsAttribLocation);
    CHECK_GL_ERRORS;
  }

	// Restore defaults
	glBindVertexArray(0);
}

void Project::setTextureMaps() {
  m_shader->setTextureMaps();
  m_depthMapShader->setTextureMaps();
}

//----------------------------------------------------------------------------------------
void Project::uploadVertexDataToVbos (
		const MeshConsolidator & meshConsolidator
) {
	// Generate VBO to store all vertex position data
	{
		glGenBuffers(1, &m_vbo_vertexPositions);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
				meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store all vertex normal data
	{
		glGenBuffers(1, &m_vbo_vertexNormals);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexNormalBytes(),
				meshConsolidator.getVertexNormalDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

  // Generate VBO to store all vertex uv data
  {
    glGenBuffers(1, &m_vbo_vertexUVs);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexUVs);

    glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexUVBytes(),
        meshConsolidator.getVertexUVDataPtr(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    CHECK_GL_ERRORS;
  }

  {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenBuffers(1, &m_vbo_screenData);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_screenData);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
  }

  {
    glGenBuffers(1, &m_vbo_vertexTangents);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexTangents);

    glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexTangentBytes(),
        meshConsolidator.getVertexTangentDataPtr(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    CHECK_GL_ERRORS;
  }

}

//----------------------------------------------------------------------------------------
void Project::mapVboDataToVertexShaderInputLocations()
{
  m_shader->mapVboDataToVertexShaderInputs(this);
  m_depthMapShader->mapVboDataToVertexShaderInputs(this);
  {
    glBindVertexArray(m_vao_screen);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_screenData);
    glVertexAttribPointer(m_aPosAttribLocation,2,GL_FLOAT,GL_FALSE, 4*sizeof(float),nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_screenData);
    glVertexAttribPointer(m_aTexCoordsAttribLocation,2,GL_FLOAT,GL_FALSE, 4*sizeof(float),(void*)(2 * sizeof(float)));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CHECK_GL_ERRORS;
  }
}

//----------------------------------------------------------------------------------------
void Project::initPerspectiveMatrix()
{
	float aspect = ((float)m_windowWidth) / m_windowHeight;
	m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
}

//----------------------------------------------------------------------------------------
void Project::initViewMatrix() {
	m_view = glm::lookAt(m_viewPos, vec3(0.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void Project::initLightSources() {
	// World-space position
	m_light.position = vec3(-7,5,-5);
	m_light.rgbIntensity = vec3(0.8f); // White light

  // glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 50.0f)
	float aspect = ((float)m_windowWidth) / m_windowHeight;
  m_lightSpaceMatrix = glm::perspective<float>(glm::radians(80.0f), 1.0f, 1.0f, 100.0f) *
    glm::lookAt(m_light.position, 
                glm::vec3( 0.0f, 0.0f,  0.0f), 
                glm::vec3( 0.0f, 1.0f,  0.0f));
}

//----------------------------------------------------------------------------------------
void Project::uploadCommonSceneUniforms() {
  m_shader->uploadCommonSceneUniforms(this);
  m_depthMapShader->uploadCommonSceneUniforms(this);
}

//TODO temporary
static int frame = 0;

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Project::appLogic()
{
	// Place per frame, application logic here ...
  frame = (frame + 1)%100;

  m_rootNode->children.front()->rotate('y', frame / 100.0);

  tickGameLogic();
  tickBubbleMovement();
}

void Project::tickGameLogic() {}

vec2 intersection(vec2 l1p, vec2 l1v, vec2 l2p, vec2 l2v) {
  float x1,y1,x2,y2,x3,y3,x4,y4;
  x1 = l1p.x; y1 = l1p.y; x2 = x1 + l1v.x; y2 = y1 + l1v.y;
  x3 = l2p.x; y3 = l2p.y; x4 = x3 + l2v.x; y4 = y3 + l2v.y;
  float a = (x1 * y2 - y1 * x2);
  float b = (x3*y4-y3*x4);
  float q = ((x1-x2) *(y3-y4) - (y1-y2) * (x3-x4));
  return vec2(
      (a*(x3-x4)-(x1-x2)*b) / q,
      (a*(y3-y4)-(y1-y2)*b) / q
      );
}

void Project::checkBBoxCollisions() {
  vec2 pp = vec2(m_playerNode->position);
  vec2 mv = glm::vec2(m_playerNode->moveVector);
  vec2 pdir = glm::normalize(mv);
  float bboxdir[4][2] = {{0,1}, {0, 1}, {1,0}, {1,0}};
  float bpts[4][2] = {{4,0}, {-4, 0}, {0,4}, {0,-4}};

  for (int i = 0; i < 4; i++) {
    vec2 bvec(bboxdir[i][0], bboxdir[i][1]); // line defining the bbox
    vec2 bpt(bpts[i][0], bpts[i][1]);
    vec2 n = glm::normalize(-bpt);
    
    vec2 btopp = pp - bpt;
    vec2 proj = bvec * glm::dot(btopp, bvec) / glm::dot(bvec, bvec) + bpt;
    float angle = glm::angle(pdir, bvec);
    if (glm::dot(btopp, n) < 0 || glm::distance(proj, pp) < m_playerNode->collisionRadius) {
      cerr << i << endl;
      vec2 iPoint = intersection(bpt, bvec, pp, pdir);
      m_playerNode->position = vec3((-pdir) * m_playerNode->collisionRadius / glm::sin(angle) + iPoint, m_playerNode->position.z);
      m_playerNode->setMoveVector(glm::vec3(mv - 2 * glm::dot(mv, n) * n,0));
    }
  }
}

void Project::checkBBlCollisions() {
  vec2 pp = vec2(m_playerNode->position);
  vec2 mv = glm::vec2(m_playerNode->moveVector);
  vec2 pdir = glm::normalize(mv);

  for (std::set<GeometryNode *>::iterator it = m_fixedBubbles.begin(); it != m_fixedBubbles.end(); it++) {
    GeometryNode *bbl = *it;
    vec2 bblpos(bbl->position);
    vec2 ptobbl(bblpos - pp);
    vec2 proj = pp + pdir * glm::dot(ptobbl, pdir) / glm::dot(pdir, pdir);

    float mindist = m_playerNode->collisionRadius + bbl->collisionRadius;
    if (glm::distance(bblpos, pp) < mindist) {
      cerr << glm::distance(bblpos, pp) << " " << m_playerNode->collisionRadius << " " << bbl->collisionRadius << endl;

      float projd = glm::distance(proj, bblpos);
      float along = glm::sqrt(mindist * mindist - projd * projd);
      float alongdir = glm::dot(mv, ptobbl) > 0 ? -1:1;
      m_playerNode->position = vec3(proj + alongdir * pdir * along, m_playerNode->position.z);
      m_playerNode->setMoveVector(vec3(0));
    }
  }
}

void Project::tickBubbleMovement() { // ASSUME only 2d collisions
  m_playerNode->move();
  
  checkBBlCollisions();
  checkBBoxCollisions();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void Project::guiLogic()
{
	if( !show_gui ) {
		return;
	}

	static bool firstRun(true);
	if (firstRun) {
		ImGui::SetNextWindowPos(ImVec2(50, 50));
		firstRun = false;
	}

	static bool showDebugWindow(true);
	ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
	float opacity(0.5f);

	ImGui::Begin("Properties", &showDebugWindow, ImVec2(100,100), opacity,
			windowFlags);


		// Add more gui elements here here ...


		// Create Button, and check if it was clicked:
		if( ImGui::Button( "Quit Application" ) ) {
			glfwSetWindowShouldClose(m_window, GL_TRUE);
		}

		ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

	ImGui::End();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Project::draw() {
	uploadCommonSceneUniforms();
	glClearColor(0.35, 0.35, 0.35, 1.0);

  CHECK_GL_ERRORS;

  // Render depth map
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  renderSceneGraph(*m_depthMapShader, *m_rootNode); // render trans for shadows
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  CHECK_GL_ERRORS;

  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_lastScreen); 
  glBindVertexArray(m_shader->m_vao);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  
  //// Render the actual scene
  glViewport(0, 0, m_windowWidth, m_windowHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, m_depthMap);

  glEnable( GL_DEPTH_TEST );
  glEnable(GL_CULL_FACE);
  renderSceneGraph(*m_shader, *m_rootNode);

  
  renderTransparentNodes(*m_shader, *m_rootNode);
  
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  

  glBindFramebuffer(GL_FRAMEBUFFER, m_blurredScreen);
  glDisable(GL_DEPTH_TEST);
  {
    m_screenShader.enable(); // TODO: cleanup
    GLuint location = m_screenShader.getUniformLocation("lastScreen");
    glUniform1i(location, 0);
    location = m_screenShader.getUniformLocation("blurredScreen");
    glUniform1i(location, 1);
    m_screenShader.disable();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_lastScreen);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_blurredScreen);
  }
  // Draw the last screen to the blurredscreen then to the real screen
  
  glBindVertexArray(m_vao_screen);

  m_screenShader.enable();
  glDrawArrays(GL_TRIANGLES, 0, 6);
  m_screenShader.disable();

  CHECK_GL_ERRORS;

  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_blurredScreen);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_blurredScreen);

  m_screenShader.enable();
  glDrawArrays(GL_TRIANGLES, 0, 6);
  m_screenShader.disable();

  CHECK_GL_ERRORS;
  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void Project::collectTransparentNodesRecursive(const SceneNode &root, const glm::mat4 &parentTransform, std::vector<std::pair<const GeometryNode *, glm::mat4> > &transparentNodes) {
  glm::mat4 myTrans = parentTransform * root.get_transform();
  for (const SceneNode * node : root.children) {
    collectTransparentNodesRecursive(*node, myTrans, transparentNodes);
  }

  if (root.m_nodeType != NodeType::GeometryNode) return;

  const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);

  if (geometryNode->material.transparency == 1) return;

  transparentNodes.push_back(make_pair(geometryNode, myTrans));
}

void Project::renderTransparentNodes(const SceneGraphShader &shader, const SceneNode &root) { 
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindVertexArray(m_shader->m_vao);
  vector<pair<const GeometryNode *,mat4>> transparentNodes;
  collectTransparentNodesRecursive(*m_rootNode, mat4(), transparentNodes);
  vector<float> centroidDistToCamera(transparentNodes.size());
  for (int i = 0; i < centroidDistToCamera.size(); i++) {
    auto p = transparentNodes[i];
    vec3 nodeCentroid = m_batchInfoMap[p.first->meshId].centroid;
    centroidDistToCamera[i] = glm::distance(vec3(p.second * vec4(nodeCentroid,1.0)), m_viewPos);
  }
  
  vector<int> tNodeId(transparentNodes.size());
  for (int i = 0; i < tNodeId.size(); i++) { tNodeId[i] = i;}

  sort(tNodeId.begin(), tNodeId.end(), [centroidDistToCamera](int i, int j) {
      return centroidDistToCamera[i] > centroidDistToCamera[j];
  });

  for (int i = 0; i < tNodeId.size(); i++) {
    auto p = transparentNodes[tNodeId[i]];
    
    m_shader->updateShaderUniforms(this, p.first, p.second, m_view);
    CHECK_GL_ERRORS;

    // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
    BatchInfo batchInfo = m_batchInfoMap[p.first->meshId];

    //-- Now render the mesh:
    m_shader->enable();
    glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
    m_shader->disable();
  }

  glBindVertexArray(0);
  glDisable(GL_CULL_FACE);

  glDisable(GL_BLEND);
}

//----------------------------------------------------------------------------------------
void Project::renderSceneGraph(const SceneGraphShader &shader, const SceneNode & root, bool rt) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
	glBindVertexArray(shader.m_vao);

  renderSceneGraphRecursive(shader, mat4(), root, rt);

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

// only renders non-transparent objects
void Project::renderSceneGraphRecursive(const SceneGraphShader &shader, const mat4 &parentTransform, const SceneNode &root, bool renderTransparent){
  glm::mat4 myTrans = parentTransform * root.get_transform();
  for (const SceneNode * node : root.children) {
    renderSceneGraphRecursive(shader, myTrans, *node, renderTransparent);
  }

  if (root.m_nodeType != NodeType::GeometryNode) return;

  const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);
  if (!renderTransparent && geometryNode->material.transparency < 1) return;

  shader.updateShaderUniforms(this, geometryNode, myTrans, m_view);

  // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
  BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

  //-- Now render the mesh:
  shader.enable();
  glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
  shader.disable();
}


//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void Project::cleanup()
{

}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool Project::cursorEnterWindowEvent (
		int entered
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool Project::mouseMoveEvent (
		double xPos,
		double yPos
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool Project::mouseButtonInputEvent (
		int button,
		int actions,
		int mods
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool Project::mouseScrollEvent (
		double xOffSet,
		double yOffSet
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool Project::windowResizeEvent (
		int width,
		int height
) {
	bool eventHandled(false);
	initPerspectiveMatrix();
	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool Project::keyInputEvent (
		int key,
		int action,
		int mods
) {
	bool eventHandled(false);

	if( action == GLFW_PRESS ) {
		if( key == GLFW_KEY_M ) {
			show_gui = !show_gui;
			eventHandled = true;
		}
    if (key == GLFW_KEY_Q) {
			glfwSetWindowShouldClose(m_window, GL_TRUE);
      eventHandled = true;
    }

	}
	// Fill in with event handling code...

	return eventHandled;
}

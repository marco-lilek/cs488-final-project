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
#include <fstream>

#include <set>
#include <string>

using namespace glm;

// NOTE: x is inverted

static const string LEVELFILE = "level.txt";
static bool show_gui = true;
static const string HIDDEN = "~hidden";
static const float SPHERE_RAD = 0.5;
static const size_t CIRCLE_PTS = 48;
static const size_t SHADOW_DIM = 1024;
static const size_t SHADOW_WIDTH = SHADOW_DIM, SHADOW_HEIGHT = SHADOW_DIM;
static const vec3 CANNON_POS(0,-6,0);

static const float ROT_SPEED = 1;
static const float ROT_MAX = 10;

static const float STARTTOP = 6;
static float boardTop = STARTTOP;
static const float BOARD_BOTTOM = -10;
static const float BOARD_SIDE = 4.75;
static float BUBBLE_SPEED = 0.2;
static float EPSILON = 0.0001;

static const float GRID_WIDTH = 9;
static const float START_GRID_HEIGHT = 11;
static float gridHeight = START_GRID_HEIGHT;
static float GRID_YOFFSET;
static const float XBOARDCORNER = 4.25;

static const size_t NUM_BUBBLETYPES = 5;
static const size_t MIN_GROUPSIZE = 3;

static vector<vector<BubbleNode *>> bubbleGrid;

static vector<SceneNode *> bubbleTypes;
static vector<vec3> btRot;

#include <cstdlib>
#include <ctime>

static const size_t TURNS_UNTIL_LOWER = 4;
static size_t curTurnsUntilLower;

static BGSoundId bgSoundId;
static const int RAND_PRECIS = 1000;

static bool cycleTypes = false;

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
	  m_vbo_vertexTangents(0),
    m_soundManager(3),
    m_cannonAngle(90),
    m_frame(0),
    m_inspecting(0),
     m_show_textures(1),
     m_show_bump(1),
     m_show_shadows(1),
     m_show_blur(1),
    m_show_transparent(1)
{
  srand(time(NULL));
  GRID_YOFFSET = SPHERE_RAD * glm::sqrt(3);
  bubbleTypes.resize(NUM_BUBBLETYPES);
  btRot.resize(NUM_BUBBLETYPES);
  for (int i = 0; i < btRot.size(); i++) {
    btRot[i].x = (1.0 - (double)(rand() % RAND_PRECIS) / RAND_PRECIS * 2) * 0.8;
    btRot[i].y = (1.0 - (double)(rand() % RAND_PRECIS) / RAND_PRECIS * 2) * 0.8;
    btRot[i].z = (1.0 - (double)(rand() % RAND_PRECIS) / RAND_PRECIS * 2) * 0.8;
  }

  bubbleGrid.resize(GRID_WIDTH);
  for (int i = 0; i < GRID_WIDTH; i++) {
    bubbleGrid[i].resize(gridHeight);
    for (int j = 0; j < gridHeight; j++) {
      bubbleGrid[i][j] = nullptr;
    }
  }

  m_viewPos = vec3(0,0,-1);
}

//----------------------------------------------------------------------------------------
// Destructor
Project::~Project()
{
  // TODO
  delete m_shader;
  delete m_depthMapShader;
  delete m_rootNode;
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
  createShader(m_skyboxShader, "DrawSkybox.vs", "DrawSkybox.fs");

	glGenVertexArrays(1, &m_vao_meshData);
	glGenVertexArrays(1, &m_vao_screen);
	glGenVertexArrays(1, &m_vao_depthData);
	glGenVertexArrays(1, &m_vao_skybox);

  enableVertexShaderInputSlots();

	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
			getAssetFilePath("cube.obj"),
			getAssetFilePath("skybox.obj"),
			getAssetFilePath("cylinder.obj"),
			getAssetFilePath("cube-sameface.obj"),
			getAssetFilePath("sphere.obj"),
			getAssetFilePath("sphere-square-uv.obj")
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

	updateLightSources();
  initViewMatrix();

  initSkybox();

  loadTextures();
  loadNoiseTexture();

  initGameLogic();

  m_soundManager.init();
  bgSoundId = m_soundManager.playBackground("background");
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

void Project::initSkybox() {
  glGenTextures(1, &m_skybox);
  glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox);

  vector<string> skyboxFaces;
  skyboxFaces.resize(6);
  string fname = "midnight-silence";
  skyboxFaces[0] = fname+"_rt.tga";
  skyboxFaces[1] = fname+"_lf.tga";
  skyboxFaces[2] = fname+"_up.tga";
  skyboxFaces[3] = fname+"_dn.tga";
  skyboxFaces[4] = fname+"_bk.tga";
  skyboxFaces[5] = fname+"_ft.tga";

  int width, height, nrChannels;
  unsigned char *data;  
  for(GLuint i = 0; i < skyboxFaces.size(); i++) {

      data = stbi_load(getAssetFilePath(skyboxFaces[i].c_str()).c_str(), &width, &height, &nrChannels, 0);
      glTexImage2D(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
          0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, data
      );
      stbi_image_free(data);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 

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

  PerlinNoise *noise = new PerlinNoise;
  char *noiseTex = noise->getNoise();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, noise->NOISE_DIM, noise->NOISE_DIM, 0, GL_RGB, GL_UNSIGNED_BYTE, noiseTex);
  delete []noiseTex;
  delete noise;
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
      glGenerateMipmap(GL_TEXTURE_2D);
    } else {
      std::cout << "Failed to load " << texPath << std::endl;
      assert(0);
    }
  }
}

void Project::loadTexturesFromList(set<string> &textures, map<string, GLuint> &nameMap, std::vector<GLuint> texs, char *defaultData) {
  DEBUGM(cerr << "generating " << textures.size() << " textures" << endl);
  texs.resize(textures.size());
  glGenTextures(textures.size(), texs.data());
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
  vector<SceneNode *> nodes;
  m_rootNode->getNodes(nodes);

  set<string> textures;
  set<string> bumpMaps;
  for (vector<SceneNode *>::iterator it = nodes.begin(); it != nodes.end(); it++) {
    if ((*it)->m_nodeType == NodeType::GeometryNode) {
      GeometryNode *node = dynamic_cast<GeometryNode *>(*it);
      textures.insert(node->texture);
      bumpMaps.insert(node->bumpMap);
    }
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
	m_rootNode = import_lua(filename);
	if (!m_rootNode) {
		std::cerr << "Could not open " << filename << std::endl;
	}
  
  vector<SceneNode *> nodes;
  m_rootNode->getNodes(nodes);
  hookControls(nodes);
}


vec3 getPosFromGrid(int i, int j) {
  return vec3(- i * SPHERE_RAD * 2 - SPHERE_RAD * (j % 2) + XBOARDCORNER, - j * GRID_YOFFSET + boardTop - SPHERE_RAD, 0);
}

void Project::hookControls(vector<SceneNode *> &nodes) {
  
  for (vector<SceneNode *>::iterator it = nodes.begin(); it != nodes.end(); it++) {
    SceneNode * node = *it;
    GeometryNode *gnode = dynamic_cast<GeometryNode *>(*it);
    if (gnode) {
      if (gnode->m_name == "~topWall") {
        m_topWall = gnode;
      }  
    } else if (node->m_name == "~cannon") {
      m_cannonNode = node;
    } else if (node->m_name == "~bubblesHolder") {
      m_bubblesHolder = node;
    }
    for (int i = 0; i < NUM_BUBBLETYPES; i++) {
      if (node->m_name == "~bubble" + std::to_string(i+1)) bubbleTypes[i] = node;
    }
  }

  for (int i = 0; i < NUM_BUBBLETYPES; i++) {
    if (bubbleTypes[i] == nullptr) {
      cerr << i << endl;
      assert(0);
    }
  }
}

void getNearestGrid(vec3 pos, int &i, int &j) {
  float approxj = (pos.y - boardTop + SPHERE_RAD) / GRID_YOFFSET * -1;

  j = glm::round(approxj);
  float approxi = (pos.x - XBOARDCORNER + SPHERE_RAD * (j % 2)) / (SPHERE_RAD * 2) * -1;
  DEBUGM(cerr << "approx j" << approxj << endl);
  DEBUGM(cerr << "approx i" << approxi << endl);

  i = glm::round(glm::clamp(approxi, 0.0f, GRID_WIDTH-1));
}


void Project::initGameLogic() {
  m_topWall->setPos(vec3(0,boardTop,0));

  /*for (int i = 0; i < GRID_WIDTH;i++) {
    for (int j = 0; j < gridHeight; j++) {
      vec3 pos = getPosFromGrid(i, j);

      GeometryNode *newBubble = new GeometryNode("sphere", "bubble");
      newBubble->scale(vec3(SPHERE_RAD));
      newBubble->setPos(pos);
      m_bubblesHolder->add_child(newBubble);
    }
  }*/
  resetBoard();
  readyBubble();
}

void Project::bubbleOffGrid() {
  m_soundManager.playSound("sad");
  resetBoard();
}

void Project::lowerTop() {
  gridHeight-=1;
  boardTop -= GRID_YOFFSET;
  m_topWall->setPos(vec3(0,boardTop,0));
  for (int i = 0; i < GRID_WIDTH;i++) {
    for (int j = 0; j < START_GRID_HEIGHT; j++) {
      if (bubbleGrid[i][j]) {
        bubbleGrid[i][j]->setPos(getPosFromGrid(i, j));
        if (j >= gridHeight) bubbleOffGrid();
      }
    }
  }
}

void Project::readyBubble() {
  static size_t bt = 0;
  BubbleNode*newBubble = new BubbleNode("bubble");
  newBubble->collisionRadius= SPHERE_RAD;
  if (cycleTypes)
    bt = (bt + 1) % NUM_BUBBLETYPES;
  else
    bt = rand() % NUM_BUBBLETYPES;

  newBubble->type = bt;
  
  newBubble->setPos(CANNON_POS);
  newBubble->add_child(bubbleTypes[bt]);

  m_newBubble = newBubble;
  m_bubblesHolder->add_child(newBubble);
}

void Project::shootBubble() {
  if (m_inspecting) return;
  if (m_newBubble->moveVector == vec3(0)) {
    m_newBubble->setMoveVector(glm::rotate(vec3(1,0,0), glm::radians(m_cannonAngle), vec3(0,0,1)) * BUBBLE_SPEED);
    curTurnsUntilLower--;
  }
}

void Project::rotateCannon(int dir) {
  if (m_inspecting) return;
  float amt = dir * ROT_SPEED;
  float pCannonAngle = m_cannonAngle;
    m_cannonAngle = glm::clamp(m_cannonAngle + amt, 0+ROT_MAX, 180-ROT_MAX);
  if (pCannonAngle != m_cannonAngle) {
    m_cannonNode->rotate('z', amt);
  }
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
  
  static const float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
  };

    glGenBuffers(1, &m_vbo_skybox);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_skybox);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    m_skyboxShader.enable(); // TODO: cleanup
    GLuint location = m_skyboxShader.getUniformLocation("skybox");
    glUniform1i(location, 0);
    m_skyboxShader.disable();
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

  {
    glBindVertexArray(m_vao_skybox);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_skybox);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
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
void Project::updateLightSources() {
  if (m_inspecting)
    m_light.position = vec3(2,2,-10);
  else
    m_light.position = vec3(glm::sin((double)m_frame/MAX_FRAME * 2 *glm::pi<double>()) * 7, 2,-10);
	m_light.rgbIntensity = vec3(0.8f); // White light

  // glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 50.0f)
	float aspect = ((float)m_windowWidth) / m_windowHeight;
  m_lightSpaceMatrix = glm::perspective<float>(glm::radians(80.0f), 1.0f, 1.0f, 100.0f) *
    glm::lookAt(m_light.position, 
                glm::vec3( 0.0f, 0.0f,  12.0f), 
                glm::vec3( 0.0f, 1.0f,  0.0f));
}

//----------------------------------------------------------------------------------------
void Project::uploadCommonSceneUniforms() {
  m_shader->uploadCommonSceneUniforms(this);
  m_depthMapShader->uploadCommonSceneUniforms(this);
}

static int skyboxFrame = 0;
static const int maxSkyboxFrame = 10000;

void Project::renderSkybox() {
  skyboxFrame = (1+ skyboxFrame) % maxSkyboxFrame;

  mat4 skyboxModel =glm::rotate(glm::radians(-15.0f), vec3(1,0,0)) * glm::rotate((float)skyboxFrame/maxSkyboxFrame* 2 *glm::pi<float>(), vec3(0,1,0));
  glDepthMask(GL_FALSE);
  m_skyboxShader.enable();

  GLint location = m_skyboxShader.getUniformLocation("model");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(skyboxModel));
  CHECK_GL_ERRORS;

  location = m_skyboxShader.getUniformLocation("view");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_view));
  CHECK_GL_ERRORS;

  location = m_skyboxShader.getUniformLocation("projection");
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
  CHECK_GL_ERRORS;

  glBindVertexArray(m_vao_skybox);
  glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox);
  glDrawArrays(GL_TRIANGLES,0,36);
  glDepthMask(GL_TRUE);

  m_skyboxShader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Project::appLogic()
{
  m_frame = (m_frame + 1)%MAX_FRAME;
  updateLightSources();

  // TODO rotation of each bubble type
  for (int i = 0; i < bubbleTypes.size(); i++) {
    bubbleTypes[i]->rotate('x', btRot[i].x);
    bubbleTypes[i]->rotate('y', btRot[i].y);
    bubbleTypes[i]->rotate('z', btRot[i].z);
  }

  tickGameLogic();
  tickBubbleMovement();
}

void Project::tickGameLogic() {
  if (glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS) rotateCannon(-1);
  else if (glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) rotateCannon(1);
}

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

bool Project::checkBBoxCollisions() {
  vec2 pp = vec2(m_newBubble->position);
  vec2 mv = glm::vec2(m_newBubble->moveVector);
  vec2 pdir = glm::normalize(mv);
  float bboxdir[4][2] = {{0,1}, {0, 1}, {1,0}, {1,0}};
  float bpts[4][2] = {{BOARD_SIDE,0}, {-BOARD_SIDE, 0}, {0,boardTop}, {0,BOARD_BOTTOM}};
  float bn[4][2] = {{-1,0}, {1, 0}, {0,-1}, {0,1}};

  bool hitTop = false;
  for (int i = 0; i < 4; i++) {
    vec2 bvec(bboxdir[i][0], bboxdir[i][1]); 
    vec2 bpt(bpts[i][0], bpts[i][1]);
    vec2 n(bn[i][0], bn[i][1]);
    
    vec2 btopp = pp - bpt;
    vec2 proj = bvec * glm::dot(btopp, bvec) / glm::dot(bvec, bvec) + bpt;
    float angle = glm::angle(pdir, bvec);
    if (glm::dot(btopp, n) < 0 || glm::distance(proj, pp) < m_newBubble->collisionRadius) {
      cerr << i << endl;
      vec2 iPoint = intersection(bpt, bvec, pp, pdir);
      m_newBubble->position = vec3((-pdir) * (m_newBubble->collisionRadius / glm::sin(angle) + EPSILON) + iPoint, m_newBubble->position.z);
      m_newBubble->setMoveVector(glm::vec3(mv - 2 * glm::dot(mv, n) * n,0));
      
      hitTop = i == 2;
    }
  }

  return hitTop;
}

void Project::inspectReady() {
  static const vec3 inspectPos = vec3(0,4,-10.6);
  if (m_inspecting) {
    m_newBubble->translate(-inspectPos);
    m_inspecting = !m_inspecting;
    return;
  }

  if (glm::dot(m_newBubble->moveVector, vec3(1,1,1)) != 0) return;

  m_newBubble->translate(inspectPos);
  m_inspecting = !m_inspecting;
}

bool Project::checkBBlCollisions() {
  vec2 pp = vec2(m_newBubble->position);
  vec2 mv = glm::vec2(m_newBubble->moveVector);
  vec2 pdir = glm::normalize(mv);

  bool hit = false;
  for (std::set<BubbleNode *>::iterator it = m_fixedBubbles.begin(); it != m_fixedBubbles.end(); it++) {
    BubbleNode *bbl = *it;
    vec2 bblpos(bbl->position);
    vec2 ptobbl(bblpos - pp);
    vec2 proj = pp + pdir * glm::dot(ptobbl, pdir) / glm::dot(pdir, pdir);

    float mindist = m_newBubble->collisionRadius + bbl->collisionRadius;
    if (glm::distance(bblpos, pp) < mindist) {
      DEBUGM(cerr << glm::distance(bblpos, pp) << " " << m_newBubble->collisionRadius << " " << bbl->collisionRadius << endl);

      float projd = glm::distance(proj, bblpos);
      float along = glm::sqrt(mindist * mindist - projd * projd) + EPSILON;
      float alongdir = glm::dot(mv, ptobbl) > 0 ? -1:1;
      m_newBubble->position = vec3(proj + alongdir * pdir * along, m_newBubble->position.z);
      DEBUGM(cerr << "newPos" << m_newBubble->position << endl);
      m_newBubble->setMoveVector(vec3(0));
      hit = true;
    }
  }
  return hit;
}

void Project::removeBubble(int i, int j) {
  BubbleNode *bubble = bubbleGrid[i][j];
  m_fixedBubbles.erase(bubble);
  m_bubblesHolder->remove_child(bubble);
  bubble->children.clear();
  delete bubble;
  bubbleGrid[i][j] = nullptr;
}

bool Project::checkForGroups(int si, int sj, size_t checkType) {
  set<pair<int, int>> frontier;
  set<pair<int,int>> seen;
  frontier.insert(make_pair(si, sj));
  seen.insert(make_pair(si,sj));

  while (!frontier.empty()) {
    auto it = frontier.begin();
    auto p = *it;
    DEBUGM(cerr << "current " << p.first << " " << p.second << endl);
    frontier.erase(it);
    const int around[6][2] = {{-1,0}, {1,0}, {0,1}, {0, -1}, 
      {(p.second % 2 == 0 ? -1 : 1),1}, {(p.second % 2 == 0 ? -1 : 1), -1}};
    for (int c = 0; c < 6; c++) {
      int ni = around[c][0] + p.first;

      int nj = around[c][1] + p.second;

      if (ni < 0 || ni >= GRID_WIDTH || nj < 0 || nj >= gridHeight) continue;
      if (bubbleGrid[ni][nj] && bubbleGrid[ni][nj]->type == checkType) {
        auto nij = make_pair(ni, nj);
        if (seen.count(nij) != 0) continue;

        seen.insert(nij);
        frontier.insert(nij);
      }
    }
  }
  DEBUGM(cerr << "group size" << seen.size() << endl);

  if (seen.size() >= MIN_GROUPSIZE) {
    m_soundManager.playSound("blop");
    for (auto it = seen.begin(); it != seen.end(); it++) {
      auto e = *it;
      removeBubble(e.first, e.second);
    }
    return true;
  }
  return false;
}

bool Project::checkForDisconnected() {
  set<pair<int, int>> frontier;
  set<pair<int,int>> seen;
  
  bool boardClear = true;
  for (int i = 0; i < GRID_WIDTH; i++) {
    if (bubbleGrid[i][0]) {
      frontier.insert(make_pair(i, 0));
      seen.insert(make_pair(i, 0));
      boardClear = false;
    }
  }
  if (boardClear) return true;

  while (!frontier.empty()) {
    auto it = frontier.begin();
    auto p = *it;
    DEBUGM(cerr << "current " << p.first << " " << p.second << endl);
    frontier.erase(it);
    const int around[6][2] = {{-1,0}, {1,0}, {0,1}, {0, -1}, 
      {(p.second % 2 == 0 ? -1 : 1),1}, {(p.second % 2 == 0 ? -1 : 1), -1}};
    for (int c = 0; c < 6; c++) {
      int ni = around[c][0] + p.first;

      int nj = around[c][1] + p.second;

      if (ni < 0 || ni >= GRID_WIDTH || nj < 0 || nj >= gridHeight) continue;
      if (bubbleGrid[ni][nj]) {
        auto nij = make_pair(ni, nj);
        if (seen.count(nij) != 0) continue;
        seen.insert(nij);
        frontier.insert(nij);
      }
    }
  }

  for (int i = 0; i < GRID_WIDTH; i++) {
    for (int j = 0; j < gridHeight; j++) {
      if (bubbleGrid[i][j] && seen.count(make_pair(i,j)) == 0) {
        removeBubble(i,j);
      }
    }
  }

  return false;
}

void Project::createBubbleAt(int i, int j, size_t type) {
  BubbleNode*newBubble = new BubbleNode("bubble");
  newBubble->collisionRadius= SPHERE_RAD;
  newBubble->type = type;
  
  newBubble->setPos(getPosFromGrid(i,j));
  m_fixedBubbles.insert(newBubble);
  bubbleGrid[i][j] = newBubble;

  newBubble->add_child(bubbleTypes[type]);
  m_bubblesHolder->add_child(newBubble);
}

void Project::tickBubbleMovement() { // ASSUME only 2d collisions
  m_newBubble->move();
  
  if (checkBBlCollisions() || checkBBoxCollisions()) {
    m_newBubble->setMoveVector(vec3(0));
    int i, j;
    getNearestGrid(m_newBubble->position, i, j);
    DEBUGM(cerr << "round " << i << " " << j << endl);

    if (j >= gridHeight) {
      m_bubblesHolder->remove_child(m_newBubble);
      m_newBubble->children.clear();
      delete m_newBubble;
      m_newBubble = nullptr;
      bubbleOffGrid();
      readyBubble();
    } else {
      /*GeometryNode *newBubble = new GeometryNode("sphere", "bubble");
      newBubble->scale(vec3(0.5));
      newBubble->setPos(m_newBubble->position);
      newBubble->material.kd = vec3(1);
      m_bubblesHolder->add_child(newBubble);
  */
      BubbleNode *tempbbl = m_newBubble;
      tempbbl->position = getPosFromGrid(i, j);

      m_fixedBubbles.insert(tempbbl);
      readyBubble();

      bubbleGrid[i][j] = tempbbl;

      if (checkForGroups(i, j, tempbbl->type) && checkForDisconnected()) {
        //m_soundManager.playSound("bazinga");
        m_soundManager.playSound("applause");
        resetBoard();
      }

      if (curTurnsUntilLower == 0) {
        curTurnsUntilLower = TURNS_UNTIL_LOWER;
        lowerTop();
      }


    }

  }
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
		ImGui::SetNextWindowPos(ImVec2(30, 50));
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

		ImGui::Text( "Framerate: %.1f FPS\n", ImGui::GetIO().Framerate );

		ImGui::Text( "Cannon angle: %.1f FPS", m_cannonAngle);

    ImGui::Text( "Inspecting (I): %d", m_inspecting);
    ImGui::Text( "Cycle Types (C): %d", cycleTypes);
    ImGui::Text( "Turns Until Lower (L): %d\n", (int)curTurnsUntilLower);

    ImGui::Text( "Textures (1): %d", m_show_textures);
    ImGui::Text( "Bump (2): %d", m_show_bump);
    ImGui::Text( "Shadows (3): %d", m_show_shadows);
    ImGui::Text( "Blur (4): %d", m_show_blur);
    ImGui::Text( "Transparent (5): %d", m_show_transparent);
    ImGui::Text("Other controls: \n(A) Toggle all\n(S) Play sound\n(B) Reset BG music\n(R) Reset");

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

  renderSkybox();

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, m_depthMap);

  glEnable( GL_DEPTH_TEST );
  //glEnable(GL_CULL_FACE);
  renderSceneGraph(*m_shader, *m_rootNode);

  
  renderTransparentNodes(*m_shader, *m_rootNode);
  
  //glDisable(GL_CULL_FACE);
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

  GLuint location = m_screenShader.getUniformLocation("doBlur");
  glUniform1i(location, m_show_blur);
  CHECK_GL_ERRORS;

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
  if (root.m_name == HIDDEN) return;
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
  /*
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
*/
  for (int i = 0; i < transparentNodes.size(); i++) {
    auto p = transparentNodes[i];
    
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
  if (root.m_name == HIDDEN) return;
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

void Project::resetBoard() {
  if (m_inspecting) inspectReady();

  curTurnsUntilLower =  TURNS_UNTIL_LOWER;
  m_cannonNode->rotate('z', 90 - m_cannonAngle);
  m_cannonAngle = 90.0f;

  boardTop = STARTTOP;
  m_topWall->setPos(vec3(0,boardTop,0));

  gridHeight = START_GRID_HEIGHT;
  for (int i = 0; i < GRID_WIDTH; i++) {
    for (int j = 0; j < gridHeight; j++) {
      if (bubbleGrid[i][j]) {
        removeBubble(i,j);
      }
    }
  }

  {
    ifstream f(getAssetFilePath(LEVELFILE.c_str()));
    for (int j = 0; j < gridHeight; j++) {
      for (int i = 0; i < GRID_WIDTH; i++) {
        int type;
        f >> type;
        if (f.eof()) goto endread;
        if (type != -1) createBubbleAt(i,j,type);
      }
    }
endread:
    cerr << "loaded from " << LEVELFILE << endl;
  }
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
      m_soundManager.stopBGSound(bgSoundId);
			glfwSetWindowShouldClose(m_window, GL_TRUE);
      eventHandled = true;
    }

    if (key == GLFW_KEY_1) {
      m_show_textures = !m_show_textures;
    }
    
    else if (key == GLFW_KEY_2) {
      m_show_bump = !m_show_bump;
    }

    else if (key == GLFW_KEY_3) {
      m_show_shadows = !m_show_shadows;
    }

    else if (key == GLFW_KEY_4) {
      m_show_blur = !m_show_blur;
    }

    else if (key == GLFW_KEY_5) {
      m_show_transparent = !m_show_transparent;
    }

    else if (key == GLFW_KEY_A) {
      m_show_blur = !m_show_blur;
      m_show_shadows = !m_show_shadows;
      m_show_bump = !m_show_bump;
      m_show_textures = !m_show_textures;
      m_show_transparent = !m_show_transparent;
      cycleTypes = !cycleTypes;
    }
    
    else if (key == GLFW_KEY_I) {
      inspectReady();
    } 
    
    else if (key == GLFW_KEY_S) {
      m_soundManager.playSound("bazinga");
    } else if (key == GLFW_KEY_B) {
      m_soundManager.stopBGSound(bgSoundId);
      bgSoundId = m_soundManager.playBackground("background");
    }

    else if (key == GLFW_KEY_L && glm::dot(m_newBubble->moveVector, vec3(1,1,1))==0) {
      lowerTop();
    } else if (key == GLFW_KEY_C) {
      cycleTypes = !cycleTypes;
    }

    else if (key == GLFW_KEY_R && glm::dot(m_newBubble->moveVector, vec3(1,1,1))==0) {

      m_show_blur = 1;
      m_show_shadows = 1;
      m_show_bump = 1;
      m_show_textures = 1;
      m_show_transparent = 1;
      cycleTypes = 0;
      resetBoard();
      m_soundManager.stopBGSound(bgSoundId);
      m_soundManager.playBackground("background");

    }

    if (key == GLFW_KEY_SPACE) {
      shootBubble();
    }

	}
	// Fill in with event handling code...

	return eventHandled;
}

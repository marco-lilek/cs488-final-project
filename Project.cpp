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


#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

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
	  m_vao_meshData(0),
	  m_vbo_vertexPositions(0),
	  m_vbo_vertexNormals(0),
	  m_vbo_vertexUVs(0)
{

}

//----------------------------------------------------------------------------------------
// Destructor
Project::~Project()
{
  delete[] m_textures;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void Project::init()
{
	// Set the background colour.
	glClearColor(0.35, 0.35, 0.35, 1.0);

	createShaderProgram();
  createDepthMapShaderProgram();

	glGenVertexArrays(1, &m_vao_meshData);
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

	mapVboDataToVertexShaderInputLocations();

  initDepthMapFBO();

	initPerspectiveMatrix();

	initLightSources();
  initViewMatrix();


  loadTextures();
  loadNoiseTexture();

	// Exiting the current scope calls delete automatically on meshConsolidator freeing
	// all vertex data resources.  This is fine since we already copied this data to
	// VBOs on the GPU.  We have no use for storing vertex data on the CPU side beyond
	// this point.
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

void Project::loadTextures() {
  // TODO: load obj with blank textures
  for (BatchInfoMap::iterator it = m_batchInfoMap.begin(); it != m_batchInfoMap.end(); it++) {
    assert(it->second.texture.size() != 0);
    m_textureNameIdMap[it->second.texture] = 0;
  }
  
  // Number of unique texture ptrs
  int numTextures = 1;//m_textureNameIdMap.size();
  DEBUGM(cerr << "numTextures " << numTextures << endl);
  m_textures = new GLuint[numTextures];
  glGenTextures(numTextures, m_textures);

  glBindTexture(GL_TEXTURE_2D, m_textures[0]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  int width, height, nrChannels;
  std::string textureFilename = "Assets/1160-normal.jpg"; //getAssetFilePath(textureName.c_str());
  DEBUGM(cerr << "loading texture " << textureFilename << endl);
  unsigned char *data = stbi_load(textureFilename.c_str(), &width, &height, &nrChannels, 0);
  if (data) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      //glGenerateMipmap(GL_TEXTURE_2D);
  } else {
      std::cout << "Failed to load texture" << std::endl;
  }
  m_textureNameIdMap[textureFilename] = m_textures[0];

  CHECK_GL_ERRORS; // we might run out of space
  glBindTexture(GL_TEXTURE_2D,0);
  /*
  unsigned int texturesIt = 0;
  for (map<string, GLuint>::iterator it = m_textureNameIdMap.begin(); it != m_textureNameIdMap.end(); it++, texturesIt++) {
    const std::string &textureName = it->first;
    m_textureNameIdMap[textureName] = m_textures[texturesIt];
    glBindTexture(GL_TEXTURE_2D, m_textures[texturesIt]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    std::string textureFilename = "Assets/container.jpg"; //getAssetFilePath(textureName.c_str());
    DEBUGM(cerr << "loading texture " << textureFilename << endl);
    unsigned char *data = stbi_load(textureFilename.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        //glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture" << std::endl;
    }

    CHECK_GL_ERRORS; // we might run out of space
    stbi_image_free(data);
  }
*/
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
}

//----------------------------------------------------------------------------------------
void Project::createShaderProgram()
{
	m_shader.generateProgramObject();
	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
	m_shader.link();
  m_shaderID = m_shader.getProgramObject();
}

//----------------------------------------------------------------------------------------
void Project::createDepthMapShaderProgram()
{
	m_depthMapShader.generateProgramObject();
	m_depthMapShader.attachVertexShader( getAssetFilePath("DepthMapVertexShader.vs").c_str() );
	m_depthMapShader.attachFragmentShader( getAssetFilePath("DepthMapFragmentShader.fs").c_str() );
	m_depthMapShader.link();
  m_depthMapShaderID = m_depthMapShader.getProgramObject();

}
//----------------------------------------------------------------------------------------
void Project::enableVertexShaderInputSlots()
{
	//-- Enable input slots for m_vao_meshData:
	{
		glBindVertexArray(m_vao_meshData);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_positionAttribLocation = m_shader.getAttribLocation("position");
		glEnableVertexAttribArray(m_positionAttribLocation);
		
    CHECK_GL_ERRORS;

		// Enable the vertex shader attribute location for "normal" when rendering.
		m_normalAttribLocation = m_shader.getAttribLocation("normal");
		glEnableVertexAttribArray(m_normalAttribLocation);

		CHECK_GL_ERRORS;
		
    // Enable the vertex shader attribute location for "uv" when rendering.
    m_uvAttribLocation = m_shader.getAttribLocation("vertexUV");
    glEnableVertexAttribArray(m_uvAttribLocation);

		CHECK_GL_ERRORS;
	}

  //input slots for depth map shader
  {
    glBindVertexArray(m_vao_depthData);
		// Enable the vertex shader attribute location for "position" when rendering.
		m_depthPositionAttribLocation = m_depthMapShader.getAttribLocation("position");
		glEnableVertexAttribArray(m_depthPositionAttribLocation);
		
    CHECK_GL_ERRORS;
  }

	// Restore defaults
	glBindVertexArray(0);
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
}

//----------------------------------------------------------------------------------------
void Project::mapVboDataToVertexShaderInputLocations()
{
  {
    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao_meshData);

    // Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
    // "position" vertex attribute location for any bound vertex shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    // Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
    // "normal" vertex attribute location for any bound vertex shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
    glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Tell GL how to map data from the vertex buffer "m_vbo_vertexUVs" into the
    // "uv" vertex attribute location for any bound vertex shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexUVs);
    glVertexAttribPointer(m_uvAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    //-- Unbind target, and restore default values:
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    CHECK_GL_ERRORS;
  }

  {
    glBindVertexArray(m_vao_depthData);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
    glVertexAttribPointer(m_depthPositionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    //-- Unbind target, and restore default values:
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
	m_view = glm::lookAt(vec3(0.0f, 5.0f, -10.0f), vec3(0.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void Project::initLightSources() {
	// World-space position
	m_light.position = vec3(-2,5,-5);
	m_light.rgbIntensity = vec3(0.8f); // White light

  // glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 50.0f)
	float aspect = ((float)m_windowWidth) / m_windowHeight;
  m_lightSpaceMatrix =glm::perspective<float>(glm::radians(45.0f), 1.0f, 2.0f, 50.0f) *
    glm::lookAt(m_light.position, 
                glm::vec3( 0.0f, 0.0f,  0.0f), 
                glm::vec3( 0.0f, 1.0f,  0.0f));
}

//----------------------------------------------------------------------------------------
void Project::uploadCommonSceneUniforms() {
	m_shader.enable();
	{
		//-- Set Perpsective matrix uniform for the scene:
		GLint location = m_shader.getUniformLocation("Perspective");
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
		CHECK_GL_ERRORS;


		//-- Set LightSource uniform for the scene:
		{
			location = m_shader.getUniformLocation("light.position");
			glUniform3fv(location, 1, value_ptr(m_light.position));
			location = m_shader.getUniformLocation("light.rgbIntensity");
			glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
			CHECK_GL_ERRORS;
		}

		//-- Set background light ambient intensity
		{
			location = m_shader.getUniformLocation("ambientIntensity");
			vec3 ambientIntensity(0.1f);
			glUniform3fv(location, 1, value_ptr(ambientIntensity));
			CHECK_GL_ERRORS;
		}

    //-- 
		{
			location = m_shader.getUniformLocation("LightSpaceMatrix");
      glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_lightSpaceMatrix));
			CHECK_GL_ERRORS;
		}


	}
	m_shader.disable();

  m_depthMapShader.enable();
  {
    GLint location = m_depthMapShader.getUniformLocation("LightSpaceMatrix");
    glUniformMatrix4fv(location,1,GL_FALSE, value_ptr(m_lightSpaceMatrix));
    CHECK_GL_ERRORS;
  }
  m_depthMapShader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void Project::appLogic()
{
	// Place per frame, application logic here ...

	uploadCommonSceneUniforms();
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
// Update mesh specific shader uniforms:
void Project::updateShaderUniforms(
		const ShaderProgram & shader,
    const GeometryNode *node,
		const glm::mat4 & nodeTrans,
		const glm::mat4 & viewMatrix
) {

	shader.enable();
	if (shader.getProgramObject() == m_shaderID) {
		//-- Set ModelView matrix:
		GLint location = shader.getUniformLocation("View");
    glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(viewMatrix));
    CHECK_GL_ERRORS;

		location = shader.getUniformLocation("Model");
    glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(nodeTrans));
    CHECK_GL_ERRORS;

		//-- Set NormMatrix:
		location = shader.getUniformLocation("NormalMatrix");
    mat3 normalMatrix = glm::transpose(glm::inverse(mat3(viewMatrix * nodeTrans)));
    glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
    CHECK_GL_ERRORS;

		//-- Set Material values:
		location = shader.getUniformLocation("material.kd");
    vec3 kd = node->material.kd;
    glUniform3fv(location, 1, value_ptr(kd));
    CHECK_GL_ERRORS;
    location = shader.getUniformLocation("material.ks");
    vec3 ks = node->material.ks;
    glUniform3fv(location, 1, value_ptr(ks));
    CHECK_GL_ERRORS;
    location = shader.getUniformLocation("material.shininess");
    glUniform1f(location, node->material.shininess);
    CHECK_GL_ERRORS;
	} else if (shader.getProgramObject() == m_depthMapShaderID) {
		GLint location = shader.getUniformLocation("Model");
    glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(nodeTrans));
    CHECK_GL_ERRORS;
  }
	shader.disable();

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void Project::draw() {
  // Render depth map
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_depthMap);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_DEPTH_BUFFER_BIT);
  glCullFace(GL_FRONT);
  renderSceneGraph(m_depthMapShader, *m_rootNode);
  glCullFace(GL_BACK);
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0); 

  //// Render the actual scene
  glViewport(0, 0, m_windowWidth, m_windowHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_shader.enable();
  GLuint location = m_shader.getUniformLocation("textureSampler");
  glUniform1i(location, 0);
  location = m_shader.getUniformLocation("shadowMap");
  glUniform1i(location, 1);
  m_shader.disable();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_textures[0]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_depthMap);
  glEnable( GL_DEPTH_TEST );
  renderSceneGraph(m_shader, *m_rootNode);
  glDisable( GL_DEPTH_TEST );

  CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void Project::renderSceneGraph(const ShaderProgram &shader, const SceneNode & root) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
	glBindVertexArray(m_vao_meshData);

  renderSceneGraphRecursive(shader, mat4(), root);

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

void Project::renderSceneGraphRecursive(const ShaderProgram &shader, const mat4 &parentTransform, const SceneNode &root) {
  glm::mat4 myTrans = parentTransform * root.get_transform();
  for (const SceneNode * node : root.children) {
    renderSceneGraphRecursive(shader, myTrans, *node);
  }

  if (root.m_nodeType != NodeType::GeometryNode) return;

  const GeometryNode * geometryNode = static_cast<const GeometryNode *>(&root);

  updateShaderUniforms(shader, geometryNode, myTrans, m_view);

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

#include "A3.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

// Lost a lot of motivation after midterms :(
// Howe yang : 20472168

static bool show_gui = true;

//Basic option bools
bool ZBUFFER = true;
bool SHOWCIRCLE = true;
bool BCULL = false;
bool FCULL = false;

const size_t CIRCLE_PTS = 48;

bool mouseleft = false;
bool mouseright = false;
bool mousemiddle = false;

double oldX;
double oldY;

mat4 resetMatrix;

//----------------------------------------------------------------------------------------
// Constructor
A3::A3(const std::string & luaSceneFile)
: m_luaSceneFile(luaSceneFile),
m_positionAttribLocation(0),
m_normalAttribLocation(0),
m_vao_meshData(0),
m_vbo_vertexPositions(0),
m_vbo_vertexNormals(0),
m_vao_arcCircle(0),
m_vbo_arcCircle(0)
{

}

//----------------------------------------------------------------------------------------
// Destructor
A3::~A3()
{

}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
 void A3::init()
 {
	// Set the background colour.
 	glClearColor(0.35, 0.35, 0.35, 1.0);

 	createShaderProgram();

 	glGenVertexArrays(1, &m_vao_arcCircle);
 	glGenVertexArrays(1, &m_vao_meshData);
 	enableVertexShaderInputSlots();

 	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
 	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
 		getAssetFilePath("cube.obj"),
 		getAssetFilePath("sphere.obj"),
 		getAssetFilePath("suzanne.obj")
 	});


	// Acquire the BatchInfoMap from the MeshConsolidator.
 	meshConsolidator->getBatchInfoMap(m_batchInfoMap);

	// Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
 	uploadVertexDataToVbos(*meshConsolidator);

 	mapVboDataToVertexShaderInputLocations();

 	initPerspectiveMatrix();

 	initViewMatrix();

 	initLightSources();

 	resetMatrix = m_rootNode->get_transform();


	// Exiting the current scope calls delete automatically on meshConsolidator freeing
	// all vertex data resources.  This is fine since we already copied this data to
	// VBOs on the GPU.  We have no use for storing vertex data on the CPU side beyond
	// this point.
 }

 //Reset funnction
 void A3::resetM(){
 	m_rootNode->set_transform(resetMatrix);
 }
//----------------------------------------------------------------------------------------
 void A3::processLuaSceneFile(const std::string & filename) {
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
 void A3::createShaderProgram()
 {
 	m_shader.generateProgramObject();
 	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
 	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
 	m_shader.link();

 	m_shader_arcCircle.generateProgramObject();
 	m_shader_arcCircle.attachVertexShader( getAssetFilePath("arc_VertexShader.vs").c_str() );
 	m_shader_arcCircle.attachFragmentShader( getAssetFilePath("arc_FragmentShader.fs").c_str() );
 	m_shader_arcCircle.link();
 }

//----------------------------------------------------------------------------------------
 void A3::enableVertexShaderInputSlots()
 {
	//-- Enable input slots for m_vao_meshData:
 	{
 		glBindVertexArray(m_vao_meshData);

		// Enable the vertex shader attribute location for "position" when rendering.
 		m_positionAttribLocation = m_shader.getAttribLocation("position");
 		glEnableVertexAttribArray(m_positionAttribLocation);

		// Enable the vertex shader attribute location for "normal" when rendering.
 		m_normalAttribLocation = m_shader.getAttribLocation("normal");
 		glEnableVertexAttribArray(m_normalAttribLocation);

 		CHECK_GL_ERRORS;
 	}


	//-- Enable input slots for m_vao_arcCircle:
 	{
 		glBindVertexArray(m_vao_arcCircle);

		// Enable the vertex shader attribute location for "position" when rendering.
 		m_arc_positionAttribLocation = m_shader_arcCircle.getAttribLocation("position");
 		glEnableVertexAttribArray(m_arc_positionAttribLocation);

 		CHECK_GL_ERRORS;
 	}

	// Restore defaults
 	glBindVertexArray(0);
 }

//----------------------------------------------------------------------------------------
 void A3::uploadVertexDataToVbos (
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

	// Generate VBO to store the trackball circle.
 	{
 		glGenBuffers( 1, &m_vbo_arcCircle );
 		glBindBuffer( GL_ARRAY_BUFFER, m_vbo_arcCircle );

 		float *pts = new float[ 2 * CIRCLE_PTS ];
 		for( size_t idx = 0; idx < CIRCLE_PTS; ++idx ) {
 			float ang = 2.0 * M_PI * float(idx) / CIRCLE_PTS;
 			pts[2*idx] = cos( ang );
 			pts[2*idx+1] = sin( ang );
 		}

 		glBufferData(GL_ARRAY_BUFFER, 2*CIRCLE_PTS*sizeof(float), pts, GL_STATIC_DRAW);

 		glBindBuffer(GL_ARRAY_BUFFER, 0);
 		CHECK_GL_ERRORS;
 	}
 }

//----------------------------------------------------------------------------------------
 void A3::mapVboDataToVertexShaderInputLocations()
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

	//-- Unbind target, and restore default values:
 	glBindBuffer(GL_ARRAY_BUFFER, 0);
 	glBindVertexArray(0);

 	CHECK_GL_ERRORS;

	// Bind VAO in order to record the data mapping.
 	glBindVertexArray(m_vao_arcCircle);

	// Tell GL how to map data from the vertex buffer "m_vbo_arcCircle" into the
	// "position" vertex attribute location for any bound vertex shader program.
 	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_arcCircle);
 	glVertexAttribPointer(m_arc_positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
 	glBindBuffer(GL_ARRAY_BUFFER, 0);
 	glBindVertexArray(0);

 	CHECK_GL_ERRORS;
 }

//----------------------------------------------------------------------------------------
 void A3::initPerspectiveMatrix()
 {
 	float aspect = ((float)m_windowWidth) / m_windowHeight;
 	m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
 }


//----------------------------------------------------------------------------------------
 void A3::initViewMatrix() {
 	m_view = glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f),
 		vec3(0.0f, 1.0f, 0.0f));
 }

//----------------------------------------------------------------------------------------
 void A3::initLightSources() {
	// World-space position
 	m_light.position = vec3(-1.0f, 3.0f, 0.5f);
	m_light.rgbIntensity = vec3(1.4f); // White light
}

//----------------------------------------------------------------------------------------
void A3::uploadCommonSceneUniforms() {
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
			vec3 ambientIntensity(0.05f);
			glUniform3fv(location, 1, value_ptr(ambientIntensity));
			CHECK_GL_ERRORS;
		}
	}
	m_shader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
 void A3::appLogic()
 {
	// Place per frame, application logic here ...

 	uploadCommonSceneUniforms();
 }

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
 void A3::guiLogic()
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

 	//ADDING
 	ImGui::Text("Toggle Options");

 	if (ImGui::Checkbox("[C]ircle ", &SHOWCIRCLE)) {

 	}
 	if (ImGui::Checkbox("[Z]-buffer", &ZBUFFER)) {

 	}
 	if (ImGui::Checkbox("[B]ackface culling", &BCULL)) {

 	}
 	if (ImGui::Checkbox("[F]rontface culling", &FCULL)) {

 	}

 	//end
 	if( ImGui::Button( "Reset Pos[I]tion" ) ) {
 		resetM();
 	}

	if( ImGui::Button( "Reset [A]ll" ) ) {
 		resetM();
 	}

		// Create Button, and check if it was clicked:
 	if( ImGui::Button( "[Q]uit Application" ) ) {
 		glfwSetWindowShouldClose(m_window, GL_TRUE);
 	}

 	ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

 	ImGui::End();
 }

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
 static void updateShaderUniforms(
 	const ShaderProgram & shader,
 	const GeometryNode & node,
 	const glm::mat4 & viewMatrix
 	) {

 	shader.enable();
 	{
		//-- Set ModelView matrix:
 		GLint location = shader.getUniformLocation("ModelView");
 		mat4 modelView = viewMatrix * node.trans;
 		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(modelView));
 		CHECK_GL_ERRORS;

		//-- Set NormMatrix:
 		location = shader.getUniformLocation("NormalMatrix");
 		mat3 normalMatrix = glm::transpose(glm::inverse(mat3(modelView)));
 		glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
 		CHECK_GL_ERRORS;


		//-- Set Material values:
 		location = shader.getUniformLocation("material.kd");
 		vec3 kd = node.material.kd;
 		glUniform3fv(location, 1, value_ptr(kd));
 		CHECK_GL_ERRORS;
 		location = shader.getUniformLocation("material.ks");
 		vec3 ks = node.material.ks;
 		glUniform3fv(location, 1, value_ptr(ks));
 		CHECK_GL_ERRORS;
 		location = shader.getUniformLocation("material.shininess");
 		glUniform1f(location, node.material.shininess);
 		CHECK_GL_ERRORS;

 	}
 	shader.disable();

 }

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
 void A3::draw() {

 	if(ZBUFFER) {
 		glEnable( GL_DEPTH_TEST );
 	}else{
 		glDisable( GL_DEPTH_TEST );
 	}
 	

 	//CULLING
 	if(BCULL || FCULL){
 		glEnable(GL_CULL_FACE);
 	}else{
 		glDisable(GL_CULL_FACE);
 	}
 	if(BCULL && FCULL){
 		glCullFace(GL_FRONT_AND_BACK);
 	}else{
 		if(BCULL){
 			glCullFace(GL_BACK);
 		}
 		if(FCULL){
 			glCullFace(GL_FRONT);
 		}
 	}
 	//need to draw after to all culling to work?
 	renderSceneGraph(*m_rootNode);


 	if(ZBUFFER){
 		glDisable( GL_DEPTH_TEST );
 	}
 	//Disable some drawing based on options in GUI
 	if(SHOWCIRCLE) {
 		renderArcCircle();
 	}


 }

//----------------------------------------------------------------------------------------
 void A3::downtree(SceneNode *node, const mat4 matrixbuffer) {

 	mat4 old = node->get_transform(); 
 	mat4 XX = matrixbuffer * old;
 	node->set_transform(XX);

//	int numofchild = node->children.size();

 	int N = 0;

	//WHY DOES SCENE NODE USE LISTS?????
	// Use iterator
 	while (node->children.size() > N){
 		std::list<SceneNode*>::iterator it = node->children.begin();
 		std::advance(it, N);
 		N = N + 1;
   		 SceneNode * child = *it; // dereference object
   		 downtree(child, XX);
   		}

   		if (node->m_nodeType == NodeType::GeometryNode) {
   			const GeometryNode *geometryNode = static_cast<const GeometryNode *>(node);

   			updateShaderUniforms(m_shader, *geometryNode, m_view);

		// Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
   			BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

		//-- Now render the mesh:
   			m_shader.enable();
   			glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
   			m_shader.disable();
   		}
//needed!
   		node->set_transform(old);
   	}

//----------------------------------------------------------------------------------------
   	void A3::renderSceneGraph(SceneNode & root) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
   		glBindVertexArray(m_vao_meshData);

   		mat4 emptybuffer = glm::mat4();
	//Recursive calls on node->children while keep and updating matrix of node

   		downtree(&root, emptybuffer);


   		glBindVertexArray(0);
   		CHECK_GL_ERRORS;
   	}

//----------------------------------------------------------------------------------------
// Draw the trackball circle.
   	void A3::renderArcCircle() {
   		glBindVertexArray(m_vao_arcCircle);

   		m_shader_arcCircle.enable();
   		GLint m_location = m_shader_arcCircle.getUniformLocation( "M" );
   		float aspect = float(m_framebufferWidth)/float(m_framebufferHeight);
   		glm::mat4 M;
   		if( aspect > 1.0 ) {
   			M = glm::scale( glm::mat4(), glm::vec3( 0.5/aspect, 0.5, 1.0 ) );
   		} else {
   			M = glm::scale( glm::mat4(), glm::vec3( 0.5, 0.5*aspect, 1.0 ) );
   		}
   		glUniformMatrix4fv( m_location, 1, GL_FALSE, value_ptr( M ) );
   		glDrawArrays( GL_LINE_LOOP, 0, CIRCLE_PTS );
   		m_shader_arcCircle.disable();

   		glBindVertexArray(0);
   		CHECK_GL_ERRORS;
   	}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
 void A3::cleanup()
 {

 }

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
 bool A3::cursorEnterWindowEvent (
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
 bool A3::mouseMoveEvent (
 	double xPos,
 	double yPos
 	) {
 	bool eventHandled(false);

	/* Fill in with event handling code...*/
 	if(mouseleft){
 		mat4 trans = translate(mat4(), vec3((oldX - xPos)*0.01, -(oldY-yPos)*0.01,0));
 		m_rootNode->set_transform(trans * m_rootNode->get_transform());
 		oldX = xPos;
 		oldY = yPos;
 	}
 	if(mousemiddle){
 		mat4 trans = translate(mat4(), vec3(0, 0,-(oldY-yPos)*0.01));
 		m_rootNode->set_transform(trans * m_rootNode->get_transform());
 		oldX = xPos;
 		oldY = yPos;
 	}
 	if(mouseright){
							//NO FKING IDEA
							//m_rootNode ->get transform for origin'ish data 
							//IDEA: move model to origin, rotate and translate back
 		mat4 test2 = m_rootNode->get_transform();
 		double x1 = test2[2][0];
 		double y1 = test2[2][1];
 		double z1 = test2[2][2];
 		m_rootNode->set_transform(m_rootNode->get_transform() * glm::rotate(mat4(), (float) ((xPos - oldX) ) * 1 /m_windowWidth, vec3(0, 0, 1)));
 		m_rootNode->set_transform(m_rootNode->get_transform() * glm::rotate(mat4(), (float) ( (yPos - oldY)) * 1 /m_windowWidth, vec3(0, 1, 0)));
 	}

 	return eventHandled;
 }

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
 bool A3::mouseButtonInputEvent (
 	int button,
 	int actions,
 	int mods
 	) {
 	bool eventHandled(false);

	// Fill in with event handling code...
 	if (actions == GLFW_PRESS) {
 		glfwGetCursorPos(m_window, &oldX, &oldY);

 		if (button == GLFW_MOUSE_BUTTON_LEFT) {
 			mouseleft = true;
 		}
 		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
 			mouseright = true;
 		}
 		if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
 			mousemiddle = true;
 		}

 	}
 	if(actions == GLFW_RELEASE){
 		mouseleft = false;
 		mouseright = false;
 		mousemiddle = false;
 	}

 	return eventHandled;
 }

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
 bool A3::mouseScrollEvent (
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
 bool A3::windowResizeEvent (
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
 bool A3::keyInputEvent (
 	int key,
 	int action,
 	int mods
 	) {
 	bool eventHandled(false);
 	//hotkeys
 	if( action == GLFW_PRESS ) {
 		if( key == GLFW_KEY_M ) {
 			show_gui = !show_gui;
 			eventHandled = true;
 		}
 		if( key == GLFW_KEY_Q ) {
 			glfwSetWindowShouldClose(m_window, GL_TRUE);
 		}
 		if( key == GLFW_KEY_I) {
 			resetM();
 		}
 		if( key == GLFW_KEY_A) {
 			resetM();
 		}
 		if( key == GLFW_KEY_C) {
 			if(SHOWCIRCLE == false){
 				SHOWCIRCLE = true;
 			}else{
 				SHOWCIRCLE = false;
 			}
 		}
 		if( key == GLFW_KEY_Z) {
 			if(ZBUFFER == false){
 				ZBUFFER = true;
 			}else{
 				ZBUFFER = false;
 			}
 		}
 		if( key == GLFW_KEY_B) {
 			if(BCULL == false){
 				BCULL = true;
 			}else{
 				BCULL = false;
 			}
 		}
 		if( key == GLFW_KEY_F) {
 			if(FCULL == false){
 				FCULL = true;
 			}else{
 				FCULL = false;
 			}
 		}
 		
 		
 	}
	// Fill in with event handling code...

 	return eventHandled;
 }

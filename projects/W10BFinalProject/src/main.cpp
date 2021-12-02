#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <sstream>
#include <typeindex>
#include <optional>
#include <string>

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

// Graphics
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureCube.h"
#include "Graphics/VertexTypes.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"

#include "BounceBehaviour.h"
//#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
		case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
		case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
		case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
			#ifdef LOG_GL_NOTIFICATIONS
		case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
			#endif
		default: break;
	}
}  

// Stores our GLFW window in a global variable for now
GLFWwindow* window;
// The current size of our window in pixels
glm::ivec2 windowSize = glm::ivec2(1920, 1080);
// The title of our GLFW window
std::string windowTitle = "INFR-1350U";

// using namespace should generally be avoided, and if used, make sure it's ONLY in cpp files
using namespace Gameplay;
using namespace Gameplay::Physics;

// The scene that we will be rendering
Scene::Sptr scene = nullptr;




void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	windowSize = glm::ivec2(width, height);
	if (windowSize.x * windowSize.y > 0) {
		scene->MainCamera->ResizeWindow(width, height);
	}
}

/// <summary>
/// Handles intializing GLFW, should be called before initGLAD, but after Logger::Init()
/// Also handles creating the GLFW window
/// </summary>
/// <returns>True if GLFW was initialized, false if otherwise</returns>
bool initGLFW() {
	// Initialize GLFW
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	//Create a new GLFW window and make it current
	window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(window);
	
	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

/// <summary>
/// Handles initializing GLAD and preparing our GLFW window for OpenGL calls
/// </summary>
/// <returns>True if GLAD is loaded, false if there was an error</returns>
bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

/// <summary>
/// Draws a widget for saving or loading our scene
/// </summary>
/// <param name="scene">Reference to scene pointer</param>
/// <param name="path">Reference to path string storage</param>
/// <returns>True if a new scene has been loaded</returns>
bool DrawSaveLoadImGui(Scene::Sptr& scene, std::string& path) {
	// Since we can change the internal capacity of an std::string,
	// we can do cool things like this!
	ImGui::InputText("Path", path.data(), path.capacity());

	// Draw a save button, and save when pressed
	if (ImGui::Button("Save")) {
		scene->Save(path);

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::SaveManifest(newFilename);
	}
	ImGui::SameLine();
	// Load scene from file button
	if (ImGui::Button("Load")) {
		// Since it's a reference to a ptr, this will
		// overwrite the existing scene!
		scene = nullptr;

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::LoadManifest(newFilename);
		scene = Scene::Load(path);

		return true;
	}
	return false;
}

/// <summary>
/// Draws some ImGui controls for the given light
/// </summary>
/// <param name="title">The title for the light's header</param>
/// <param name="light">The light to modify</param>
/// <returns>True if the parameters have changed, false if otherwise</returns>
bool DrawLightImGui(const Scene::Sptr& scene, const char* title, int ix) {
	bool isEdited = false;
	bool result = false;
	Light& light = scene->Lights[ix];
	ImGui::PushID(&light); // We can also use pointers as numbers for unique IDs
	if (ImGui::CollapsingHeader(title)) {
		isEdited |= ImGui::DragFloat3("Pos", &light.Position.x, 0.01f);
		isEdited |= ImGui::ColorEdit3("Col", &light.Color.r);
		isEdited |= ImGui::DragFloat("Range", &light.Range, 0.1f);

		result = ImGui::Button("Delete");
	}
	if (isEdited) {
		scene->SetShaderLight(ix);
	}

	ImGui::PopID();
	return result;
}

/// <summary>
/// Draws a simple window for displaying materials and their editors
/// </summary>
void DrawMaterialsWindow() {
	if (ImGui::Begin("Materials")) {
		ResourceManager::Each<Material>([](Material::Sptr material) {
			material->RenderImGui();
		});
	}
	ImGui::End();
}

/// <summary>
/// handles creating or loading the scene
/// </summary>
/// 
glm::vec3 black = glm::vec3(0.0f);
glm::vec3 yellow = glm::vec3(1.0f, 1.0f, 0.0f);
std::vector<Guid> edgeID;
void CreateScene() {
	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene) {
		ResourceManager::LoadManifest("manifest.json");
		scene = Scene::Load("scene.json");

		// Call scene awake to start up all of our components
		scene->Window = window;
		scene->Awake();
	}
	else {
		// This time we'll have 2 different shaders, and share data between both of them using the UBO
		// This shader will handle reflective materials 
		Shader::Sptr reflectiveShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_environment_reflective.glsl" }
		});

		// This shader handles our basic materials without reflections (cause they expensive)
		Shader::Sptr basicShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }
		});

		// This shader handles our basic materials without reflections (cause they expensive)
		Shader::Sptr specShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/textured_specular.glsl" }
		});


		///////////////////// NEW SHADERS ////////////////////////////////////////////

		// This shader handles our foliage vertex shader example
		Shader::Sptr foliageShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/screendoor_transparency.glsl" }
		});

		// This shader handles our cel shading example
		Shader::Sptr toonShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/toon_shading.glsl" }
		});


		// Load in the meshes
		//// Table
		MeshResource::Sptr mesh_table = ResourceManager::CreateAsset<MeshResource>("gObj_table/table.obj");
		MeshResource::Sptr mesh_table_plane = ResourceManager::CreateAsset<MeshResource>("gObj_table/table_plane.obj");
		Texture2D::Sptr tex_table = ResourceManager::CreateAsset<Texture2D>("gObj_table/tex_table.png");
		Texture2D::Sptr tex_white = ResourceManager::CreateAsset<Texture2D>("textures/blankTexture.jpg");
		Texture2D::Sptr tex_black = ResourceManager::CreateAsset<Texture2D>("textures/blackTexture.jpg");

		//// Puck
		MeshResource::Sptr mesh_puck = ResourceManager::CreateAsset<MeshResource>("gObj_puck/puck.obj");
		Texture2D::Sptr tex_puck = ResourceManager::CreateAsset<Texture2D>("gObj_puck/GoldenDark2.jpg");

		//// Paddle
		MeshResource::Sptr mesh_paddle = ResourceManager::CreateAsset<MeshResource>("gObj_paddle/paddle.obj");
		MeshResource::Sptr mesh_paddle2 = ResourceManager::CreateAsset<MeshResource>("gObj_paddle/paddle.obj");
		Texture2D::Sptr tex_paddle_red = ResourceManager::CreateAsset<Texture2D>("gObj_paddle/Red.jpg");
		Texture2D::Sptr tex_paddle_blue = ResourceManager::CreateAsset<Texture2D>("gObj_paddle/Blue.jpg");
		 
		//// Edge
		MeshResource::Sptr mesh_edge1 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge2 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge3 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge4 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge5 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge6 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge7 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge8 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge9 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge10 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge11 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");
		MeshResource::Sptr mesh_edge12 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edge_uni.obj");

		MeshResource::Sptr mesh_edgeS1 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edgeS1.obj");
		MeshResource::Sptr mesh_edgeS2 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edgeS2.obj");
		MeshResource::Sptr mesh_edgeS3 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edgeS3.obj");
		MeshResource::Sptr mesh_edgeS4 = ResourceManager::CreateAsset<MeshResource>("gObj_edge/edgeS4.obj");
		Texture2D::Sptr tex_edgeSkin = ResourceManager::CreateAsset<Texture2D>("gObj_edge/tex_edge.png");

		MeshResource::Sptr mesh_edgeMask = ResourceManager::CreateAsset<MeshResource>("gObj_edge/bagua.obj");

		// Load in some textures
		/*
		Texture2D::Sptr    boxTexture = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);*/

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		Shader::Sptr      skyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		// Create an empty scene
		scene = std::make_shared<Scene>();

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap);
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Create our materials
		
		Material::Sptr material_white = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_white->Name = "White";
			material_white->Set("u_Material.Diffuse", tex_white);
			material_white->Set("u_Material.Shininess", 256.0f);
		}

		Material::Sptr material_black = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_black->Name = "Black";
			material_black->Set("u_Material.Diffuse", tex_black);
			material_black->Set("u_Material.Shininess", 10.0f);
		}


		//// Table
		Material::Sptr material_table = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_table->Name = "Table";
			material_table->Set("u_Material.Diffuse", tex_table);
			material_table->Set("u_Material.Shininess", 256.0f);
		}

		//// Puck
		Material::Sptr material_puck = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_puck->Name = "Puck";
			material_puck->Set("u_Material.Diffuse", tex_puck);
			material_puck->Set("u_Material.Shininess", 256.0f);
		}

		//// Paddle
		Material::Sptr material_paddle = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_paddle->Name = "Paddle";
			material_paddle->Set("u_Material.Diffuse", tex_paddle_red);
			material_paddle->Set("u_Material.Shininess", 256.0f);
		}

		Material::Sptr material_paddle2 = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_paddle2->Name = "Paddle2";
			material_paddle2->Set("u_Material.Diffuse", tex_paddle_blue);
			material_paddle2->Set("u_Material.Shininess", 256.0f);
		}

		//// Edge
		Material::Sptr material_edge = ResourceManager::CreateAsset<Material>(basicShader);
		{
			material_edge->Name = "Edge";
			material_edge->Set("u_Material.Diffuse", tex_edgeSkin);
			material_edge->Set("u_Material.Shininess", 256.0f);
		}

		//// Lights ////
#pragma region Lights Creation
// Create some lights for our scene
		scene->Lights.resize(10);

		// Main Light
		scene->Lights[0].Position = glm::vec3(0.0f, 0.0f, 10.0f);
		scene->Lights[0].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[0].Range = 300.0f;

		// Light 9 follows puck

		// l1
		scene->Lights[1].Range = 2.0f;
		scene->Lights[2].Range = 2.0f;
		scene->Lights[3].Range = 2.0f;
		scene->Lights[4].Range = 2.0f;
		scene->Lights[5].Range = 2.0f;
		scene->Lights[6].Range = 2.0f;
		scene->Lights[7].Range = 2.0f;
		scene->Lights[8].Range = 2.0f;
		scene->Lights[9].Range = -13.0f;

		scene->Lights[1].Color = black;
		scene->Lights[2].Color = black;
		scene->Lights[3].Color = black;
		scene->Lights[4].Color = black;
		scene->Lights[5].Color = black;
		scene->Lights[6].Color = black;
		scene->Lights[7].Color = black;
		scene->Lights[8].Color = black;
		scene->Lights[9].Color = glm::vec3(0.043f, 0.043f, 0.0f);
		
		scene->Lights[1].Position = glm::vec3(-14.840f, 13.340f, -6.560f);
		scene->Lights[2].Position = glm::vec3(-14.720f, -12.980f, -6.620f);
		scene->Lights[3].Position = glm::vec3(-4.460f, 15.460f, -6.590f);
		scene->Lights[4].Position = glm::vec3(-4.450f, -15.180f, -6.600f);

		scene->Lights[5].Position = glm::vec3(15.330f, 13.320f, -6.600f);
		scene->Lights[6].Position = glm::vec3(15.040f, -13.320f, -6.570f);
		scene->Lights[7].Position = glm::vec3(4.350f, 15.380f, -6.570f);
		scene->Lights[8].Position = glm::vec3(4.540f, -15.540f, -6.570f);

		scene->Lights[9].Position = glm::vec3(0.0f, 0.0f, 10.0f);

		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		// Set up the scene's camera
		/*GameObject::Sptr camera = scene->CreateGameObject("Main Camera");
		{
			camera->SetPostion(glm::vec3(5.0f));
			camera->LookAt(glm::vec3(0.0f));

			camera->Add<SimpleCameraControl>();

			Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			scene->MainCamera = cam;
		}

		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(boxMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>();
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		GameObject::Sptr monkey1 = scene->CreateGameObject("Monkey 1");
		{
			// Set position in the scene
			monkey1->SetPostion(glm::vec3(1.5f, 0.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey1->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey1->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Add a dynamic rigid body to this monkey
			RigidBody::Sptr physics = monkey1->Add<RigidBody>(RigidBodyType::Dynamic);
			physics->AddCollider(ConvexMeshCollider::Create());
		}

		GameObject::Sptr monkey2 = scene->CreateGameObject("Complex Object");
		{
			// Set and rotation position in the scene
			monkey2->SetPostion(glm::vec3(-1.5f, 0.0f, 1.0f));
			monkey2->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			// Add a render component
			RenderComponent::Sptr renderer = monkey2->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(boxMaterial);

			// This is an example of attaching a component and setting some parameters
			RotatingBehaviour::Sptr behaviour = monkey2->Add<RotatingBehaviour>();
			behaviour->RotationSpeed = glm::vec3(0.0f, 0.0f, -90.0f);
		}

		// Box to showcase the specular material
		GameObject::Sptr specBox = scene->CreateGameObject("Specular Object");
		{
			MeshResource::Sptr boxMesh = ResourceManager::CreateAsset<MeshResource>();
			boxMesh->AddParam(MeshBuilderParam::CreateCube(ZERO, ONE));
			boxMesh->GenerateMesh();

			// Set and rotation position in the scene
			specBox->SetPostion(glm::vec3(0, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = specBox->Add<RenderComponent>();
			renderer->SetMesh(boxMesh);
			renderer->SetMaterial(testMaterial);
		}

		///// NEW OBJECTS ////

		// sphere to showcase the foliage material
		GameObject::Sptr foliageBall = scene->CreateGameObject("Foliage Sphere");
		{
			MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
			sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 8));
			sphere->GenerateMesh();

			// Set and rotation position in the scene
			foliageBall->SetPostion(glm::vec3(-4.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(foliageMaterial);
		}

		// Box to showcase the foliage material
		GameObject::Sptr foliageBox = scene->CreateGameObject("Foliage Box");
		{
			MeshResource::Sptr box = ResourceManager::CreateAsset<MeshResource>();
			box->AddParam(MeshBuilderParam::CreateCube(glm::vec3(0, 0, 0.5f) , ONE));
			box->GenerateMesh();

			// Set and rotation position in the scene
			foliageBox->SetPostion(glm::vec3(-6.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBox->Add<RenderComponent>();
			renderer->SetMesh(box);
			renderer->SetMaterial(foliageMaterial);
		}

		// Box to showcase the specular material
		GameObject::Sptr toonBall = scene->CreateGameObject("Toon Object");
		{
			MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
			sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 8));
			sphere->GenerateMesh();

			// Set and rotation position in the scene
			toonBall->SetPostion(glm::vec3(-2.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = toonBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(toonMaterial);
		}

		// Kinematic rigid bodies are those controlled by some outside controller
		// and ONLY collide with dynamic objects
		RigidBody::Sptr physics = monkey2->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(ConvexMeshCollider::Create());

		// Create a trigger volume for testing how we can detect collisions with objects!
		GameObject::Sptr trigger = scene->CreateGameObject("Trigger");
		{
			TriggerVolume::Sptr volume = trigger->Add<TriggerVolume>();
			CylinderCollider::Sptr collider = CylinderCollider::Create(glm::vec3(3.0f, 3.0f, 1.0f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.5f));
			volume->AddCollider(collider);

			trigger->Add<TriggerVolumeEnterBehaviour>();
		}
		*/

		//// gObj Setup ////
#pragma region Scene Object Setup
//// Camera
		GameObject::Sptr camera = scene->CreateGameObject("Main Camera");
		{
			camera->SetPostion(glm::vec3(0.0f, -3.0f, 13.0f));
			camera->LookAt(glm::vec3(0.0f));
			camera->SetRotation(glm::vec3(8.0f, 0.0f, 0.0f));

			Camera::Sptr cam = camera->Add<Camera>();

			// Make sure that the camera is set as the scene's main camera!
			scene->MainCamera = cam;
		}

		//// Table
		GameObject::Sptr gObj_table = scene->CreateGameObject("Base Table");
		{
			gObj_table->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_table->Add<RenderComponent>();
			renderer->SetMesh(mesh_table);
			renderer->SetMaterial(material_table);

			RigidBody::Sptr physics = gObj_table->Add<RigidBody>(RigidBodyType::Static);
		}
		GameObject::Sptr gObj_table_plane = scene->CreateGameObject("Table_plane");
		{
			gObj_table_plane->SetPostion(glm::vec3(0.0f, 0.0f, -8.01f));

			RenderComponent::Sptr renderer = gObj_table_plane->Add<RenderComponent>();
			renderer->SetMesh(mesh_table_plane);
			renderer->SetMaterial(material_table);

			RigidBody::Sptr physics = gObj_table_plane->Add<RigidBody>(RigidBodyType::Static);
			physics->AddCollider(PlaneCollider::Create());
		}

		//// Puck
		GameObject::Sptr gObj_puck = scene->CreateGameObject("Puck");
		{
			gObj_puck->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			gObj_puck->SetPostion(glm::vec3(0.0f, 0.0f, 4.0f));


			RenderComponent::Sptr renderer = gObj_puck->Add<RenderComponent>();
			renderer->SetMesh(mesh_puck);
			renderer->SetMaterial(material_puck);

			RigidBody::Sptr physics = gObj_puck->Add<RigidBody>(RigidBodyType::Dynamic);
			ICollider::Sptr collider = physics->AddCollider(ConvexMeshCollider::Create());

			BounceBehaviour::Sptr bounceTrigger = gObj_puck->Add<BounceBehaviour>();
			//bounceTrigger->rigidOBJ = physics;
		}

		//// Paddle_red
		GameObject::Sptr gObj_paddle_red = scene->CreateGameObject("Paddle_red");
		{
			gObj_paddle_red->SetPostion(glm::vec3(-5.0f, 0.0f, -8.01f));
			gObj_paddle_red->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));


			RenderComponent::Sptr renderer = gObj_paddle_red->Add<RenderComponent>();
			renderer->SetMesh(mesh_paddle);
			renderer->SetMaterial(material_paddle);

			RigidBody::Sptr physics = gObj_paddle_red->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(ConvexMeshCollider::Create());
		}

		//// Paddle_blue
		GameObject::Sptr gObj_paddle_blue = scene->CreateGameObject("Paddle_blue");
		{
			gObj_paddle_blue->SetPostion(glm::vec3(5.0f, 0.0f, -8.01f));
			gObj_paddle_blue->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));


			RenderComponent::Sptr renderer = gObj_paddle_blue->Add<RenderComponent>();
			renderer->SetMesh(mesh_paddle2);
			renderer->SetMaterial(material_paddle2);

			RigidBody::Sptr physics = gObj_paddle_blue->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(ConvexMeshCollider::Create());
		}
		//// Edge
#pragma region 12 Edges
		//RigidBody::Sptr edgePhys;

		GameObject::Sptr gObj_edge1 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge1->GUID);
			gObj_edge1->SetPostion(glm::vec3(-17.230f, 5.540f, -8.02f));
			gObj_edge1->SetRotation(glm::vec3(0.0f, 0.0f, -93.5f));
			gObj_edge1->SetScale(glm::vec3(2.980f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge1->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge1);
			renderer->SetMaterial(material_white);

			TriggerVolume::Sptr volume = gObj_edge1->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge1->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(2.980f, 1.0f, 3.0f));
		}
		

		GameObject::Sptr gObj_edge2 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge2->GUID);
			gObj_edge2->SetPostion(glm::vec3(-17.230f, -5.540f, -8.02f));
			gObj_edge2->SetRotation(glm::vec3(0.0f, 0.0f, -86.5f));
			gObj_edge2->SetScale(glm::vec3(2.980f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge2->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge2);
			renderer->SetMaterial(material_white);

			/*
			TriggerVolume::Sptr volume = gObj_edge2->Add<TriggerVolume>();
			ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			collider->SetScale(glm::vec3(2.980f, 1.0f, 3.0f));*/

			TriggerVolume::Sptr volume = gObj_edge2->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge2->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(2.980f, 1.0f, 3.0f)
				);
		}
		

		GameObject::Sptr gObj_edge3 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge3->GUID);
			gObj_edge3->SetPostion(glm::vec3(-12.790f, 11.280f, -8.02f));
			gObj_edge3->SetRotation(glm::vec3(0.0f, 0.0f, -147.1));
			gObj_edge3->SetScale(glm::vec3(5.080f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge3->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge3);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge3->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(5.080f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge3->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge3->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(5.080f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge4 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge4->GUID);
			gObj_edge4->SetPostion(glm::vec3(-12.790f, -11.280f, -8.02f));
			gObj_edge4->SetRotation(glm::vec3(0.0f, 0.0f, -32.9f));
			gObj_edge4->SetScale(glm::vec3(5.080f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge4->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge4);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge4->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(5.080f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge4->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge4->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(5.080f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge5 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge5->GUID);
			gObj_edge5->SetPostion(glm::vec3(-4.210f, 12.800f, -8.02f));
			gObj_edge5->SetRotation(glm::vec3(0.0f, 0.0f, 163.7f));
			gObj_edge5->SetScale(glm::vec3(4.430f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge5->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge5);
			renderer->SetMaterial(material_white);


			//TriggerVolume::Sptr volume = gObj_edge5->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(4.430f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge5->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge5->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(4.430f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge6 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge6->GUID);
			gObj_edge6->SetPostion(glm::vec3(-4.210f, -12.800f, -8.02f));
			gObj_edge6->SetRotation(glm::vec3(0.0f, 0.0f, 16.3f));
			gObj_edge6->SetScale(glm::vec3(4.430f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge6->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge6);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge6->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(4.430f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge6->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge6->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(4.430f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge7 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge7->GUID);
			gObj_edge7->SetPostion(glm::vec3(4.210f, 12.800f, -8.02f));
			gObj_edge7->SetRotation(glm::vec3(0.0f, 0.0f, -163.7f));
			gObj_edge7->SetScale(glm::vec3(4.430f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge7->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge7);
			renderer->SetMaterial(material_white);


			//TriggerVolume::Sptr volume = gObj_edge7->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(4.430f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge7->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge7->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(4.430f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge8 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge8->GUID);
			gObj_edge8->SetPostion(glm::vec3(4.210f, -12.800f, -8.02f));
			gObj_edge8->SetRotation(glm::vec3(0.0f, 0.0f, -16.3f));
			gObj_edge8->SetScale(glm::vec3(4.430f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge8->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge8);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge8->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(4.430f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge8->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge8->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(4.430f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge9 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge9->GUID);
			gObj_edge9->SetPostion(glm::vec3(12.790f, 11.280f, -8.02f));
			gObj_edge9->SetRotation(glm::vec3(0.0f, 0.0f, 147.1f));
			gObj_edge9->SetScale(glm::vec3(5.080f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge9->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge9);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge9->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(5.080f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge9->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge9->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(5.080f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge10 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge10->GUID);
			gObj_edge10->SetPostion(glm::vec3(12.790f, -11.280f, -8.02f));
			gObj_edge10->SetRotation(glm::vec3(0.0f, 0.0f, 32.9f));
			gObj_edge10->SetScale(glm::vec3(5.080f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge10->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge10);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge10->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(5.080f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge10->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge10->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(5.080f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge11 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge11->GUID);
			gObj_edge11->SetPostion(glm::vec3(17.230f, 5.540f, -8.02f));
			gObj_edge11->SetRotation(glm::vec3(0.0f, 0.0f, 93.5f));
			gObj_edge11->SetScale(glm::vec3(2.980f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge11->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge11);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge11->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(2.980f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge11->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge11->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(2.980f, 1.0f, 3.0f)
				);
		}
		GameObject::Sptr gObj_edge12 = scene->CreateGameObject("Edge");
		{
			edgeID.push_back(gObj_edge12->GUID);
			gObj_edge12->SetPostion(glm::vec3(17.230f, -5.540f, -8.02f));
			gObj_edge12->SetRotation(glm::vec3(0.0f, 0.0f, 86.5f));
			gObj_edge12->SetScale(glm::vec3(2.980f, 1.0f, 1.0f));
			RenderComponent::Sptr renderer = gObj_edge12->Add<RenderComponent>();
			renderer->SetMesh(mesh_edge12);
			renderer->SetMaterial(material_white);

			//TriggerVolume::Sptr volume = gObj_edge12->Add<TriggerVolume>();
			//ICollider::Sptr collider = volume->AddCollider(ConvexMeshCollider::Create());
			//collider->SetScale(glm::vec3(2.980f, 1.0f, 3.0f));

			TriggerVolume::Sptr volume = gObj_edge12->Add<TriggerVolume>();
			RigidBody::Sptr physics = gObj_edge12->Add<RigidBody>(RigidBodyType::Static);
			volume->AddCollider(
				physics->AddCollider(ConvexMeshCollider::Create()))->SetScale(glm::vec3(2.980f, 1.0f, 3.0f)
				);
		}



#pragma endregion



		//// Edge Skin
		GameObject::Sptr gObj_edgeS1 = scene->CreateGameObject("Edge_skin1");
		{
			gObj_edgeS1->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_edgeS1->Add<RenderComponent>();
			renderer->SetMesh(mesh_edgeS1);
			renderer->SetMaterial(material_edge);
		}
		GameObject::Sptr gObj_edgeS2 = scene->CreateGameObject("Edge_skin2");
		{
			gObj_edgeS2->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_edgeS2->Add<RenderComponent>();
			renderer->SetMesh(mesh_edgeS2);
			renderer->SetMaterial(material_edge);
		}
		GameObject::Sptr gObj_edgeS3 = scene->CreateGameObject("Edge_skin3");
		{
			gObj_edgeS3->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_edgeS3->Add<RenderComponent>();
			renderer->SetMesh(mesh_edgeS3);
			renderer->SetMaterial(material_edge);
		}
		GameObject::Sptr gObj_edgeS4 = scene->CreateGameObject("Edge_skin4");
		{
			gObj_edgeS4->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_edgeS4->Add<RenderComponent>();
			renderer->SetMesh(mesh_edgeS4);
			renderer->SetMaterial(material_edge);
		}



		//// Edge Mask
		GameObject::Sptr gObj_edgeMask = scene->CreateGameObject("Edge_mask");
		{
			gObj_edgeMask->SetPostion(glm::vec3(0.0f, 0.0f, -8.0f));

			RenderComponent::Sptr renderer = gObj_edgeMask->Add<RenderComponent>();
			renderer->SetMesh(mesh_edgeMask);
			renderer->SetMaterial(material_black);
		}

		//// Plane 

		//// ENDREGION
#pragma endregion


		// Call scene awake to start up all of our components
		scene->Window = window;
		scene->Awake();

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");
	}
}

double f0_cursorPosX, f0_cursorPosY;
Guid myID;


void scoreCheck(bool isLeft);
void scoreCheckReset();
void checkIsReseting();
void lightReset();



int lScore = 0;
int rScore = 0;

bool resetCheck = false;
bool isWin = false;

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	// Initialize our ImGui helper
	ImGuiHelper::Init(window);

	// Initialize our resource manager
	ResourceManager::Init();

	// Register all our resource types so we can load them from manifest files
	ResourceManager::RegisterType<Texture2D>();
	ResourceManager::RegisterType<TextureCube>();
	ResourceManager::RegisterType<Shader>();
	ResourceManager::RegisterType<Material>();
	ResourceManager::RegisterType<MeshResource>();

	// Register all of our component types so we can load them from files
	ComponentManager::RegisterType<Camera>();
	ComponentManager::RegisterType<RenderComponent>();
	ComponentManager::RegisterType<RigidBody>();
	ComponentManager::RegisterType<TriggerVolume>();
	ComponentManager::RegisterType<RotatingBehaviour>();
	ComponentManager::RegisterType<JumpBehaviour>();
	ComponentManager::RegisterType<MaterialSwapBehaviour>();
	ComponentManager::RegisterType<TriggerVolumeEnterBehaviour>();
	ComponentManager::RegisterType<SimpleCameraControl>();
	ComponentManager::RegisterType<BounceBehaviour>();

	// GL states, we'll enable depth testing and backface fulling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	// Structure for our frame-level uniforms, matches layout from
	// fragments/frame_uniforms.glsl
	// For use with a UBO.
	struct FrameLevelUniforms {
		// The camera's view matrix
		glm::mat4 u_View;
		// The camera's projection matrix
		glm::mat4 u_Projection;
		// The combined viewProject matrix
		glm::mat4 u_ViewProjection;
		// The camera's position in world space
		glm::vec4 u_CameraPos;
		// The time in seconds since the start of the application
		float u_Time;
	};
	// This uniform buffer will hold all our frame level uniforms, to be shared between shaders
	UniformBuffer<FrameLevelUniforms>::Sptr frameUniforms = std::make_shared<UniformBuffer<FrameLevelUniforms>>(BufferUsage::DynamicDraw);
	// The slot that we'll bind our frame level UBO to
	const int FRAME_UBO_BINDING = 0;

	// Structure for our isntance-level uniforms, matches layout from
	// fragments/frame_uniforms.glsl
	// For use with a UBO.
	struct InstanceLevelUniforms {
		// Complete MVP
		glm::mat4 u_ModelViewProjection;
		// Just the model transform, we'll do worldspace lighting
		glm::mat4 u_Model;
		// Normal Matrix for transforming normals
		glm::mat4 u_NormalMatrix;
	};

	// This uniform buffer will hold all our instance level uniforms, to be shared between shaders
	UniformBuffer<InstanceLevelUniforms>::Sptr instanceUniforms = std::make_shared<UniformBuffer<InstanceLevelUniforms>>(BufferUsage::DynamicDraw);

	// The slot that we'll bind our instance level UBO to
	const int INSTANCE_UBO_BINDING = 1;

	////////////////////////////////
	///// SCENE CREATION MOVED /////
	////////////////////////////////
	CreateScene();

	// We'll use this to allow editing the save/load path
	// via ImGui, note the reserve to allocate extra space
	// for input!
	std::string scenePath = "scene.json"; 
	scenePath.reserve(256); 

	// Our high-precision timer
	double lastFrame = glfwGetTime();

	BulletDebugMode physicsDebugMode = BulletDebugMode::None;
	float playbackSpeed = 1.0f;


	nlohmann::json editorSceneState;

	bool isFirstClick = true;
	float countDown = 2;

	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGuiHelper::StartFrame();

		// modify position of these two.... - Justin Lee: "seems location not matter much, so I just place it here."
		checkIsReseting();
		scoreCheckReset();
		// Don't change the order of these two function

		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		// Draw our material properties window!
		DrawMaterialsWindow();

		// Showcasing how to use the imGui library!
		bool isDebugWindowOpen = ImGui::Begin("Debugging");
		if (isDebugWindowOpen) {
			// Draws a button to control whether or not the game is currently playing
			static char buttonLabel[64];
			sprintf_s(buttonLabel, "%s###playmode", scene->IsPlaying ? "Exit Play Mode" : "Enter Play Mode");
			if (ImGui::Button(buttonLabel)) {
				// Save scene so it can be restored when exiting play mode
				if (!scene->IsPlaying) {
					editorSceneState = scene->ToJson();
				}

				// Toggle state
				scene->IsPlaying = !scene->IsPlaying;

				// If we've gone from playing to not playing, restore the state from before we started playing
				if (!scene->IsPlaying) {
					scene = nullptr;
					// We reload to scene from our cached state
					scene = Scene::FromJson(editorSceneState);
					// Don't forget to reset the scene's window and wake all the objects!
					scene->Window = window;
					scene->Awake();
				}
			}

			// Make a new area for the scene saving/loading
			ImGui::Separator();
			if (DrawSaveLoadImGui(scene, scenePath)) {
				// C++ strings keep internal lengths which can get annoying
				// when we edit it's underlying datastore, so recalcualte size
				scenePath.resize(strlen(scenePath.c_str()));

				// We have loaded a new scene, call awake to set
				// up all our components
				scene->Window = window;
				scene->Awake();
			}
			ImGui::Separator();
			// Draw a dropdown to select our physics debug draw mode
			if (BulletDebugDraw::DrawModeGui("Physics Debug Mode:", physicsDebugMode)) {
				scene->SetPhysicsDebugDrawMode(physicsDebugMode);
			}
			LABEL_LEFT(ImGui::SliderFloat, "Playback Speed:    ", &playbackSpeed, 0.0f, 10.0f);
			ImGui::Separator();
		}

		// Clear the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw some ImGui stuff for the lights
		if (isDebugWindowOpen) {
			for (int ix = 0; ix < scene->Lights.size(); ix++) {
				char buff[256];
				sprintf_s(buff, "Light %d##%d", ix, ix);
				// DrawLightImGui will return true if the light was deleted
				if (DrawLightImGui(scene, buff, ix)) {
					// Remove light from scene, restore all lighting data
					scene->Lights.erase(scene->Lights.begin() + ix);
					scene->SetupShaderAndLights();
					// Move back one element so we don't skip anything!
					ix--;
				}
			}
			// As long as we don't have max lights, draw a button
			// to add another one
			if (scene->Lights.size() < scene->MAX_LIGHTS) {
				if (ImGui::Button("Add Light")) {
					scene->Lights.push_back(Light());
					scene->SetupShaderAndLights();
				}
			}
			// Split lights from the objects in ImGui
			ImGui::Separator();
		}

		dt *= playbackSpeed;

		// Perform updates for all components
		scene->Update(dt);

		// Grab shorthands to the camera and shader from the scene
		Camera::Sptr camera = scene->MainCamera;

		// Cache the camera's viewprojection
		glm::mat4 viewProj = camera->GetViewProjection();
		DebugDrawer::Get().SetViewProjection(viewProj);

		// Update our worlds physics!
		scene->DoPhysics(dt);

		// Draw object GUIs
		if (isDebugWindowOpen) {
			scene->DrawAllGameObjectGUIs();
		}
		
		// The current material that is bound for rendering
		Material::Sptr currentMat = nullptr;
		Shader::Sptr shader = nullptr;

		// Bind the skybox texture to a reserved texture slot
		// See Material.h and Material.cpp for how we're reserving texture slots
		TextureCube::Sptr environment = scene->GetSkyboxTexture();
		if (environment) environment->Bind(0); 

		// Here we'll bind all the UBOs to their corresponding slots
		scene->PreRender();
		frameUniforms->Bind(FRAME_UBO_BINDING);
		instanceUniforms->Bind(INSTANCE_UBO_BINDING);

		// Upload frame level uniforms
		auto& frameData = frameUniforms->GetData();
		frameData.u_Projection = camera->GetProjection();
		frameData.u_View = camera->GetView();
		frameData.u_ViewProjection = camera->GetViewProjection();
		frameData.u_CameraPos = glm::vec4(camera->GetGameObject()->GetPosition(), 1.0f);
		frameData.u_Time = static_cast<float>(thisFrame);
		frameUniforms->Update();

		// Render all our objects
		ComponentManager::Each<RenderComponent>([&](const RenderComponent::Sptr& renderable) {
			// Early bail if mesh not set
			if (renderable->GetMesh() == nullptr) { 
				return;
			}

			// If we don't have a material, try getting the scene's fallback material
			// If none exists, do not draw anything
			if (renderable->GetMaterial() == nullptr) {
				if (scene->DefaultMaterial != nullptr) {
					renderable->SetMaterial(scene->DefaultMaterial);
				} else {
					return;
				}
			}

			// If the material has changed, we need to bind the new shader and set up our material and frame data
			// Note: This is a good reason why we should be sorting the render components in ComponentManager
			if (renderable->GetMaterial() != currentMat) {
				currentMat = renderable->GetMaterial();
				shader = currentMat->GetShader();

				shader->Bind();
				currentMat->Apply();
			}

			// Grab the game object so we can do some stuff with it
			GameObject* object = renderable->GetGameObject();
			 
			// Use our uniform buffer for our instance level uniforms
			auto& instanceData = instanceUniforms->GetData();
			instanceData.u_Model = object->GetTransform();
			instanceData.u_ModelViewProjection = viewProj * object->GetTransform();
			instanceData.u_NormalMatrix = glm::mat3(glm::transpose(glm::inverse(object->GetTransform())));
			instanceUniforms->Update();  

			// Draw the object
			renderable->GetMesh()->Draw();
		});
		/// <summary>
		/// puck interaction
		/// </summary>
		/// <returns></returns>
		GameObject::Sptr gObj_puck = scene->FindObjectByName("Puck");
		RigidBody::Sptr rigid_puck = gObj_puck->Get<RigidBody>();

		while (countDown > 0)
			countDown -= dt;


		/*
		if (gObj_puck->Get<BounceBehaviour>()->isInCollision && countDown <= 0)
		{
			rigid_puck->resetVelocity();
			rigid_puck->ApplyImpulse(gObj_puck->Get<BounceBehaviour>()->repelVelocity);
			countDown = 1;
		}*/

		

		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		{
			//std::cout << "APPLYING FORCE" << std::endl;
			rigid_puck->ApplyForce(glm::vec3(0.0f, 10.0f, 0.0f));
		}
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		{
			//std::cout << "APPLYING FORCE" << std::endl;
			rigid_puck->ApplyForce(glm::vec3(0.0f, -10.0f, 0.0f));
		}

		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		{
			//std::cout << "APPLYING FORCE" << std::endl;
			rigid_puck->ApplyForce(glm::vec3(10.0f, 0.0f, 0.0f));
		}

		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		{
			//std::cout << "APPLYING FORCE" << std::endl;
			rigid_puck->ApplyForce(glm::vec3(-10.0f, 0.0f, 0.0f));
		}

		/// <summary>
		/// Red Paddle Control
		/// </summary>
		/// <returns></returns>
		GameObject::Sptr paddle_R = scene->FindObjectByName("Paddle_red");
		if (glfwGetMouseButton(window, 0) == GLFW_PRESS)
		{
			if (!isFirstClick)
			{
				double cursorX;
				double cursorY;
				glfwGetCursorPos(window, &cursorX, &cursorY);

				int windowX;
				int windowY;
				glfwGetWindowSize(window, &windowX, &windowY);
				double doub_winX = (double)windowX / 2;
				double doub_winY = (double)windowY / 2;

				cursorX = cursorX / doub_winX;
				cursorY = cursorY / doub_winY;

				f0_cursorPosX = f0_cursorPosX / doub_winX;
				f0_cursorPosY = f0_cursorPosY / doub_winY;

				glm::vec3 dCursorPos = glm::vec3(((float)(cursorX - f0_cursorPosX)), -((float)(cursorY - f0_cursorPosY)), 0.0f);
				paddle_R->SetPostion((paddle_R->GetPosition() + dCursorPos * 20.0f));
			}
			else
			{
				glfwGetCursorPos(window, &f0_cursorPosX, &f0_cursorPosY);
				isFirstClick = false;
			}
		}
		glfwGetCursorPos(window, &f0_cursorPosX, &f0_cursorPosY);

		/// <summary>
		/// Paddle B control
		/// </summary>
		/// <returns></returns>
		GameObject::Sptr paddle_B = scene->FindObjectByName("Paddle_blue");
		glm::vec3 pbPos = paddle_B->GetPosition();
		float keyMoveSpeed = 0.1f;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		{
			paddle_B->SetPostion(glm::vec3(pbPos.x, pbPos.y + keyMoveSpeed, pbPos.z));
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		{
			paddle_B->SetPostion(glm::vec3(pbPos.x - keyMoveSpeed, pbPos.y, pbPos.z));
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			paddle_B->SetPostion(glm::vec3(pbPos.x, pbPos.y - keyMoveSpeed, pbPos.z));
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		{
			paddle_B->SetPostion(glm::vec3(pbPos.x + keyMoveSpeed, pbPos.y, pbPos.z));
		}

		// Apply colliding
		BounceBehaviour::Sptr bounce_puck = scene->FindObjectByName("Puck")->Get<BounceBehaviour>();

		glm::vec3 puckPos = gObj_puck->GetPosition();
		if (puckPos.x <= -17.6f) // RIGHT WINS 
		{
			scoreCheck(false);
			gObj_puck->SetPostion(glm::vec3(9.5f, 0.0f, -1.0f));
			rigid_puck->resetVelocity();
		}
		if (puckPos.x >= 17.6f) // LEFT WINS
		{
			scoreCheck(true);
			gObj_puck->SetPostion(glm::vec3(-9.5f, 0.0f, -1.0f));
			rigid_puck->resetVelocity();
		}

		scene->Lights[9].Position = glm::vec3(gObj_puck->GetPosition().x, gObj_puck->GetPosition().y, gObj_puck->GetPosition().z + 3.0f);
		


		scene->SetupShaderAndLights();
		// Use our cubemap to draw our skybox
		scene->DrawSkybox();

		// End our ImGui window
		ImGui::End();

		VertexArrayObject::Unbind();

		lastFrame = thisFrame;
		ImGuiHelper::EndFrame();
		glfwSwapBuffers(window);
	}

	// Clean up the ImGui library
	ImGuiHelper::Cleanup();

	// Clean up the resource manager
	ResourceManager::Cleanup();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}

void scoreCheck(bool isLeft) {
	if (isLeft) {
		lScore += 1;
		scene->Lights[lScore].Color = yellow;
	}
	else
	{
		rScore += 1;
		scene->Lights[rScore + 4].Color = yellow;
	}
}

void scoreCheckReset() {
	if (lScore >= 4)
	{
		std::cout << "Red Player Wins !!!!!!!!!" << std::endl;

		lScore = 0;
		rScore = 0;
		resetCheck = true;
	}
	if (rScore >= 4)
	{
		std::cout << "Blue Player Wins !!!!!!!!!" << std::endl;

		lScore = 0;
		rScore = 0;
		resetCheck = true;
	}
}

void checkIsReseting() {
	GameObject::Sptr gObj_puck = scene->FindObjectByName("Puck");
	if (resetCheck == true) {
		std::cout << "RESETING GAME!!!!!" << std::endl;
		gObj_puck->SetPostion(glm::vec3(0.0f, 0.0f, 4.0f));
		lightReset();
		resetCheck = false;
		Sleep(1000);
	}
}

void lightReset()
{
	for (int i = 1; i < 9; i++)
	{
		scene->Lights[i].Color = black;
	}
}

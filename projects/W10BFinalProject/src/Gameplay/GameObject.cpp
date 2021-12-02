#include "GameObject.h"

// Utilities
#include "Utils/JsonGlmHelpers.h"

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include "GLM/gtc/matrix_transform.hpp"
#include "GLM/gtc/quaternion.hpp"
#include "GLM/glm.hpp"
#include "Utils/GlmDefines.h"
#include "Utils/ImGuiHelper.h"

#include "Gameplay/Scene.h"

namespace Gameplay {
	GameObject::GameObject() :
		Name("Unknown"),
		GUID(Guid::New()),
		_components(std::vector<IComponent::Sptr>()),
		_scene(nullptr),
		_position(ZERO),
		_rotation(glm::quat(glm::vec3(0.0f))),
		_scale(ONE),
		_transform(MAT4_IDENTITY),
		_inverseTransform(MAT4_IDENTITY),
		_isTransformDirty(true)
	{ }

	void GameObject::_RecalcTransform() const
	{
		if (_isTransformDirty) {
			_transform = glm::translate(MAT4_IDENTITY, _position) * glm::mat4_cast(_rotation) * glm::scale(MAT4_IDENTITY, _scale);
			_inverseTransform = glm::inverse(_transform);
			_isTransformDirty = false;
		}
	}

	void GameObject::LookAt(const glm::vec3& point) {
		glm::mat4 rot = glm::lookAt(_position, point, glm::vec3(0.0f, 0.0f, 1.0f));
		// Take the conjugate of the quaternion, as lookAt returns the *inverse* rotation
		SetRotation(glm::conjugate(glm::quat_cast(rot)));
	}


	void GameObject::OnEnteredTrigger(const std::shared_ptr<Physics::TriggerVolume>& trigger) {
		for (auto& component : _components) {
			component->OnEnteredTrigger(trigger);
		}
	}

	void GameObject::OnLeavingTrigger(const std::shared_ptr<Physics::TriggerVolume>& trigger) {
		for (auto& component : _components) {
			component->OnLeavingTrigger(trigger);
		}
	}

	void GameObject::OnTriggerVolumeEntered(const std::shared_ptr<Physics::RigidBody>& trigger) {
		for (auto& component : _components) {
			component->OnTriggerVolumeEntered(trigger);
		}
	}

	void GameObject::OnTriggerVolumeLeaving(const std::shared_ptr<Physics::RigidBody>& trigger) {
		for (auto& component : _components) {
			component->OnTriggerVolumeLeaving(trigger);
		}
	}

	void GameObject::SetPostion(const glm::vec3& position) {
		_position = position;
		_isTransformDirty = true;
	}

	const glm::vec3& GameObject::GetPosition() const {
		return _position;
	}

	void GameObject::SetRotation(const glm::quat& value) {
		_rotation = value;
		_isTransformDirty = true;
	}

	const glm::quat& GameObject::GetRotation() const {
		return _rotation;
	}

	void GameObject::SetRotation(const glm::vec3& eulerAngles) {
		_rotation = glm::quat(glm::radians(eulerAngles));
		_isTransformDirty = true;
	}

	glm::vec3 GameObject::GetRotationEuler() const {
		return glm::degrees(glm::eulerAngles(_rotation));
	}

	void GameObject::SetScale(const glm::vec3& value) {
		_scale = value;
		_isTransformDirty = true;
	}

	const glm::vec3& GameObject::GetScale() const {
		return _scale;
	}

	const glm::mat4& GameObject::GetTransform() const {
		_RecalcTransform();
		return _transform;
	}


	const glm::mat4& GameObject::GetInverseTransform() const {
		_RecalcTransform();
		return _inverseTransform;
	}

	Scene* GameObject::GetScene() const {
		return _scene;
	}

	void GameObject::Awake() {
		for (auto& component : _components) {
			component->Awake();
		}
	}

	void GameObject::Update(float dt) {
		for (auto& component : _components) {
			if (component->IsEnabled) {
				component->Update(dt);
			}
		}
	}

	bool GameObject::Has(const std::type_index& type) {
		// Iterate over all the pointers in the components list
		for (const auto& ptr : _components) {
			// If the pointer type matches T, we return true
			if (std::type_index(typeid(*ptr.get())) == type) {
				return true;
			}
		}
		return false;
	}

	std::shared_ptr<IComponent> GameObject::Get(const std::type_index& type)
	{
		// Iterate over all the pointers in the binding list
		for (const auto& ptr : _components) {
			// If the pointer type matches T, we return that behaviour, making sure to cast it back to the requested type
			if (std::type_index(typeid(*ptr.get())) == type) {
				return ptr;
			}
		}
		return nullptr;
	}

	std::shared_ptr<IComponent> GameObject::Add(const std::type_index& type)
	{
		LOG_ASSERT(!Has(type), "Cannot add 2 instances of a component type to a game object");

		// Make a new component, forwarding the arguments
		std::shared_ptr<IComponent> component = ComponentManager::Create(type);
		// Let the component know we are the parent
		component->_context = this;

		// Append it to the binding component's storage, and invoke the OnLoad
		_components.push_back(component);
		component->OnLoad();

		if (_scene->GetIsAwake()) {
			component->Awake();
		}

		return component;
	}

	void GameObject::DrawImGui() {

		ImGui::PushID(this); // Push a new ImGui ID scope for this object
		// Since we're allowing names to change, we need to use the ### to have a static ID for the header
		static char buffer[256];
		sprintf_s(buffer, 256, "%s###GO_HEADER", Name.c_str());
		if (ImGui::CollapsingHeader(buffer)) {
			ImGui::Indent();

			// Draw a textbox for our name
			static char nameBuff[256];
			memcpy(nameBuff, Name.c_str(), Name.size());
			nameBuff[Name.size()] = '\0';
			if (ImGui::InputText("", nameBuff, 256)) {
				Name = nameBuff;
			}
			ImGui::SameLine();
			if (ImGuiHelper::WarningButton("Delete")) {
				ImGui::OpenPopup("Delete GameObject");
			}

			// Draw our delete modal
			if (ImGui::BeginPopupModal("Delete GameObject")) {
				ImGui::Text("Are you sure you want to delete this game object?");
				if (ImGuiHelper::WarningButton("Yes")) {
					// Remove ourselves from the scene
					_scene->RemoveGameObject(SelfRef());

					// Restore imgui state so we can early bail
					ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
					ImGui::Unindent();
					ImGui::PopID();
					return;
				}
				ImGui::SameLine();
				if (ImGui::Button("No")) {
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			// Render position label
			_isTransformDirty |= LABEL_LEFT(ImGui::DragFloat3, "Position", &_position.x, 0.01f);
			
			// Get the ImGui storage state so we can avoid gimbal locking issues by storing euler angles in the editor
			glm::vec3 euler = GetRotationEuler();
			ImGuiStorage* guiStore = ImGui::GetStateStorage();

			// Extract the angles from the storage, note that we're only using the address of the position for unique IDs
			euler.x = guiStore->GetFloat(ImGui::GetID(&_position.x), euler.x);
			euler.y = guiStore->GetFloat(ImGui::GetID(&_position.y), euler.y);
			euler.z = guiStore->GetFloat(ImGui::GetID(&_position.z), euler.z);

			//Draw the slider for angles
			if (LABEL_LEFT(ImGui::DragFloat3, "Rotation", &euler.x, 1.0f)) {
				// Wrap to the -180.0f to 180.0f range for safety
				euler = Wrap(euler, -180.0f, 180.0f);

				// Update the editor state with our new values
				guiStore->SetFloat(ImGui::GetID(&_position.x), euler.x);
				guiStore->SetFloat(ImGui::GetID(&_position.y), euler.y);
				guiStore->SetFloat(ImGui::GetID(&_position.z), euler.z);

				//Send new rotation to the gameobject
				SetRotation(euler);
			}
			
			// Draw the scale
			_isTransformDirty |= LABEL_LEFT(ImGui::DragFloat3, "Scale   ", &_scale.x, 0.01f, 0.0f);

			ImGui::Separator();
			ImGui::TextUnformatted("Components");
			ImGui::Separator();

			// Render each component under it's own header
			for (int ix = 0; ix < _components.size(); ix++) {
				std::shared_ptr<IComponent> component = _components[ix];
				if (ImGui::CollapsingHeader(component->ComponentTypeName().c_str())) {
					ImGui::PushID(component.get()); 
					component->RenderImGui();
					// Render a delete button for the component
					if (ImGuiHelper::WarningButton("Delete")) {
						_components.erase(_components.begin() + ix);
						ix--;
					}
					ImGui::PopID();
				}
			}
			ImGui::Separator();

			// Render a combo box for selecting a component to add
			static std::string preview = "";
			static std::optional<std::type_index> selectedType;
			if (ImGui::BeginCombo("##AddComponents", preview.c_str())) {
				ComponentManager::EachType([&](const std::string& typeName, const std::type_index type) {
					// Hide component types already added
					if (!Has(type)) {
						bool isSelected = typeName == preview;
						if (ImGui::Selectable(typeName.c_str(), &isSelected)) {
							preview = typeName;
							selectedType = type;
						}
					}
				});
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			// Button to add component and reset the selected type
			if (ImGui::Button("Add Component") && selectedType.has_value() && !Has(selectedType.value())) {
				Add(selectedType.value());
				selectedType.reset();
				preview = "";
			}

			ImGui::Separator();

			ImGui::Unindent();
		}
		ImGui::PopID(); // Pop the ImGui ID scope for the object
	}

	std::shared_ptr<GameObject> GameObject::SelfRef() {
		return _selfRef.lock();
	}

	GameObject::Sptr GameObject::FromJson(const nlohmann::json& data)
	{
		// We need to manually construct since the GameObject constructor is
		// protected. We can call it here since Scene is a friend class of GameObjects
		GameObject::Sptr result(new GameObject());

		// Load in basic info
		result->Name = data["name"];
		result->GUID = Guid(data["guid"]);
		result->_position = ParseJsonVec3(data["position"]);
		result->_rotation = ParseJsonQuat(data["rotation"]);
		result->_scale    = ParseJsonVec3(data["scale"]);
		result->_isTransformDirty = true;

		// Since our components are stored based on the type name, we iterate
		// on the keys and values from the components object
		nlohmann::json components = data["components"];
		for (auto& [typeName, value] : components.items()) {
			// We need to reference the component registry to load our components
			// based on the type name (note that all component types need to be
			// registered at the start of the application)
			IComponent::Sptr component = ComponentManager::Load(typeName, value);
			component->_context = result.get();

			// Add component to object and allow it to perform self initialization
			result->_components.push_back(component);
			component->OnLoad();
		}
		return result;
	}

	nlohmann::json GameObject::ToJson() const {
		nlohmann::json result = {
			{ "name", Name },
			{ "guid", GUID.str() },
			{ "position", GlmToJson(_position) },
			{ "rotation", GlmToJson(_rotation) },
			{ "scale",    GlmToJson(_scale) },
		};
		result["components"] = nlohmann::json();
		for (auto& component : _components) {
			result["components"][component->ComponentTypeName()] = component->ToJson();
			IComponent::SaveBaseJson(component, result["components"][component->ComponentTypeName()]);
		}
		return result;
	}
}

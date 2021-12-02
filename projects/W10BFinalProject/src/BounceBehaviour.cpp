#include "BounceBehaviour.h"
#include "Gameplay/Components/ComponentManager.h"
#include "Gameplay/GameObject.h"


BounceBehaviour::BounceBehaviour() :
	IComponent(),
	gameObj(nullptr),
	rigidOBJ(nullptr),
	isInCollision(false)
{ }
BounceBehaviour::~BounceBehaviour() = default;

void BounceBehaviour::OnEnteredTrigger(const std::shared_ptr<Gameplay::Physics::TriggerVolume>&trigger)
{
	printf("OnEnteredTrigger\n");
	if (rigidOBJ && gameObj->Name == "Puck") {
		isInCollision = true;

		glm::vec3 rigiVelo = rigidOBJ->GetVelocity();
		rigiVelo.z = 0.0f;

		glm::vec3 edgeVec;
		glm::vec3 forwVec = glm::vec3(0.0f, 1.0f, 0.0f); // forward normal
		glm::quat rot = trigger->GetGameObject()->GetRotation(); // rotation angle of the edge
		edgeVec = glm::vec3(glm::vec4(forwVec, 1.0) * rot); // rotate normal
		edgeVec.x *= -1.0f; // turn inside
		edgeVec.z = 0.0f;

		float speed = glm::length(rigiVelo);

		rigiVelo = glm::normalize(rigiVelo);
		edgeVec = glm::normalize(edgeVec);

		glm::vec3 refVelo = glm::reflect(rigiVelo, edgeVec);
		refVelo.z = 0.0f;

		refVelo *= speed * 0.5f;

		rigidOBJ->resetVelocity();
		rigidOBJ->ApplyImpulse(refVelo);
		reflectionVelocity = refVelo;
		repelVelocity = glm::normalize(refVelo) * 10.0f;
	}
}

void BounceBehaviour::OnLeavingTrigger(const std::shared_ptr<Gameplay::Physics::TriggerVolume>&trigger)
{

	//rigidOBJ->resetVelocity();
	//rigidOBJ->ApplyImpulse(reflectionVelocity);
	isInCollision = false;
	printf("OnLeavingTrigger\n");
}

void BounceBehaviour::OnTriggerVolumeEntered(const std::shared_ptr<Gameplay::Physics::RigidBody>&trigger) {
	/* // This part works weird. So it is removed.
	if (gameObj->Name == "Edge") {
		std::cout << "EDGE BOUNCE" << std::endl;

		glm::vec3 edgeVec;
		glm::vec3 forwVec = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::quat rot = trigger->GetGameObject()->GetRotation();
		edgeVec = glm::vec3(glm::vec4(forwVec, 1.0) * rot);
		edgeVec.x *= -1.0f;
		edgeVec.z = 0.0f;
		edgeVec = glm::normalize(edgeVec);

		float speed = 5.0f;

		glm::vec3 repelVelo = edgeVec * speed;

		trigger->resetVelocity();
		trigger->ApplyImpulse(repelVelo);
	}
	*/
	printf("OnTriggerVolumeEntered\n");
}

void BounceBehaviour::OnTriggerVolumeLeaving(const std::shared_ptr<Gameplay::Physics::RigidBody>&trigger)
{
	printf("OnTriggerVolumeLeaving\n");

}

void BounceBehaviour::Awake() {
	gameObj = GetGameObject();
	if (gameObj->Name == "Puck") {
		rigidOBJ = GetComponent<Gameplay::Physics::RigidBody>();
	}

}

void BounceBehaviour::RenderImGui() {
	// no need to render it
}


nlohmann::json BounceBehaviour::ToJson() const {
	return {
		// nothing
	};
}

BounceBehaviour::Sptr BounceBehaviour::FromJson(const nlohmann::json & blob) {
	BounceBehaviour::Sptr result = std::make_shared<BounceBehaviour>();
	return result;
}

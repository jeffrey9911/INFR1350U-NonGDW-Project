#pragma once
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Physics/TriggerVolume.h"
//#include "Gameplay/Physics/RigidBody.h"

class BounceBehaviour : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<BounceBehaviour> Sptr;
	BounceBehaviour();
	virtual ~BounceBehaviour();

	Gameplay::GameObject* gameObj;
	Gameplay::Physics::RigidBody::Sptr rigidOBJ;
	bool isInCollision;
	glm::vec3 reflectionVelocity;
	glm::vec3 repelVelocity;

	/// <summary>
	/// Overide
	/// Bounce behaviour on onEnteredTrigger
	/// </summary>
	/// <param name="trigger"></param>
	virtual void OnEnteredTrigger(const std::shared_ptr<Gameplay::Physics::TriggerVolume>& trigger) override;

	virtual void OnLeavingTrigger(const std::shared_ptr<Gameplay::Physics::TriggerVolume>& trigger) override;

	virtual void OnTriggerVolumeEntered(const std::shared_ptr<Gameplay::Physics::RigidBody>& trigger) override;

	virtual void OnTriggerVolumeLeaving(const std::shared_ptr<Gameplay::Physics::RigidBody>& trigger) override;




	virtual void Awake() override;
	virtual void RenderImGui() override;
	virtual nlohmann::json ToJson() const override;
	static BounceBehaviour::Sptr FromJson(const nlohmann::json& blob);


	MAKE_TYPENAME(BounceBehaviour);
};
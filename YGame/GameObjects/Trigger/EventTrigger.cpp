#include "EventTrigger.h"

#include "Systems/GameTime/GameTime.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/OBB/OBBCollider.h"

void EventTrigger::Initialize(Camera* camera) {
	camera_ = camera;
	InitCollision();
	InitJson();
}

void EventTrigger::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kEventTrigger)
	);
	// トリガーは通り抜け扱い
	obbCollider_->SetEnablePenetration(true);
}

void EventTrigger::Update() {
	wt_.UpdateMatrix();
	if (obbCollider_) {
		obbCollider_->Update();
	}
	if (action_) {
		action_->Update(YoRigine::GameTime::GetDeltaTime());
	}
}

void EventTrigger::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

void EventTrigger::SetAction(std::unique_ptr<TriggerAction> action) {
	action_ = std::move(action);
	if (action_) {
		action_->OnAttach(this);
	}
}

void EventTrigger::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (action_) action_->OnTriggerEnter(other);
}

void EventTrigger::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (action_) action_->OnTriggerStay(other);
}

void EventTrigger::OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (action_) action_->OnTriggerExit(other);
}

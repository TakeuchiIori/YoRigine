#include "EventTrigger.h"

#include "Systems/GameTime/GameTime.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/OBB/OBBCollider.h"

void EventTrigger::Initialize(YoRigine::Camera* camera) {
	camera_ = camera;
	// WorldTransform は D3D12 マップポインタ (transformData_) を内部で持つため、
	// 必ず Initialize を通さないと UpdateMatrix で nullptr deref する。
	wt_.Initialize();
	wt_.UpdateMatrix();
	InitCollision();
	InitJson();
}

void EventTrigger::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kEventTrigger)
	);
	if (!obbCollider_) return; // プール枯渇等で null の場合は無効状態のまま続行

	// トリガーはプレイヤー/敵を物理的に押し戻さない (penetration 解決から除外する)
	// CollisionManager::ResolvePenetration では片側でも false なら resolve をスキップする。
	obbCollider_->SetEnablePenetration(false);
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

#pragma once

#include "Object3d/BaseObject.h"
#include "TriggerAction.h"

#include <memory>
#include <string>

// ============================================================
// EventTrigger
//   任意のイベント発火点として置けるトリガーオブジェクト。
//   - 自身は描画しない (OBBコライダーのみ持つ)。
//   - 実際の応答処理は差し替え可能な TriggerAction に委譲する。
//
//   使い方:
//     auto trigger = std::make_unique<EventTrigger>();
//     trigger->Initialize(camera);
//     trigger->SetAction(std::make_unique<OpenGateAction>(...));
//     // 毎フレーム trigger->Update() を呼ぶ
// ============================================================
class EventTrigger : public BaseObject {
public:
	EventTrigger() = default;
	~EventTrigger() override = default;

	void Initialize(Camera* camera) override;
	void Update() override;
	void Draw() override {}
	void DrawCollision() override;

	// Action を後から差し込む。OnAttach が呼ばれる。
	void SetAction(std::unique_ptr<TriggerAction> action);

	// シーン上で識別するための名前 (デバッグ・参照用)。コライダー有効化等とは無関係。
	void SetName(const std::string& name) { name_ = name; }
	const std::string& GetName() const { return name_; }

	// BaseObject の衝突コールバックを Action へ委譲
	void OnEnterCollision(BaseCollider* self, BaseCollider* other) override;
	void OnCollision     (BaseCollider* self, BaseCollider* other) override;
	void OnExitCollision (BaseCollider* self, BaseCollider* other) override;

private:
	void InitCollision() override;
	void InitJson() override {}  // MVP では JSON 連携なし

private:
	std::unique_ptr<TriggerAction> action_;
	std::string name_;
};

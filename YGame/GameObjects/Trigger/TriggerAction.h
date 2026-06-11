#pragma once

#include "Collision/Core/BaseCollider.h"

class EventTrigger;

// ============================================================
// TriggerAction
//   EventTrigger に差し込む応答ロジックの抽象基底。
//   サブクラスは必要なフックだけオーバーライドする。
//     - Update            : 毎フレーム呼ばれる (外部状態のポーリング用)
//     - OnTriggerEnter    : 何かがコライダーに侵入した瞬間
//     - OnTriggerStay     : 侵入中に毎フレーム
//     - OnTriggerExit     : 抜けた瞬間
// ============================================================
class TriggerAction {
public:
	virtual ~TriggerAction() = default;

	// EventTrigger に attach された直後に1回呼ばれる。owner ポインタを保持する用途。
	virtual void OnAttach(EventTrigger* owner) { owner_ = owner; }

	virtual void Update(float /*deltaTime*/) {}

	virtual void OnTriggerEnter(BaseCollider* /*other*/) {}
	virtual void OnTriggerStay (BaseCollider* /*other*/) {}
	virtual void OnTriggerExit (BaseCollider* /*other*/) {}

protected:
	EventTrigger* owner_ = nullptr;
};

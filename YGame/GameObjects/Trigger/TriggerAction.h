#pragma once

#include "Collision/Core/BaseCollider.h"

#include <json.hpp>
#include <string>

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

	// シリアライズ用: 自身を JSON ("action" ノード相当) に書き出す。"type" を必ず含めること。
	virtual nlohmann::json SerializeToJson() const = 0;

	// 種別名 (Factory の dispatch キー / エディタ表示用)。
	virtual std::string GetTypeName() const = 0;

	// このアクションが EventTrigger の位置/回転/スケールを使うか。
	// 衝突で発火する系 (BattleZone / Teleport / Checkpoint 等) は true。
	// シグナル駆動 (OpenGate のように撃破通知だけ見る) は false にすると
	// エディタが空間パラメータの編集 UI を非表示にする。
	virtual bool NeedsSpatialPlacement() const { return true; }

protected:
	EventTrigger* owner_ = nullptr;
};

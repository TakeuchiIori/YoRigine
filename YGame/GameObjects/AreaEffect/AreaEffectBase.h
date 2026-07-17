#pragma once

// App
#include "IAreaEffectTarget.h"

// Engine
#include "Collision/AreaCollision/CircleArea.h"

// C++
#include <memory>
#include <vector>

class Line;

// ============================================================
// エリアエフェクトの基底クラス（ゲーム側）。
//   CircleArea を1つ所有し、対象の Enter/Stay/Exit を
//   派生クラスの仮想関数フックへ橋渡しする。
//   派生は効果（OnStay 等）だけを実装すればよい。
//   駆動は AreaEffectManager が担う（自前で対象を集めない）。
// ============================================================
class AreaEffectBase {
public:
	// 円判定に掛ける対象1件（AreaEffectManager が構築して渡す）
	struct Target {
		IAreaEffectTarget* ptr = nullptr;
		Vector3            position;
	};

public:
	virtual ~AreaEffectBase() = default;

	// 円の中心・半径・Stay tick 間隔(秒, 0=毎フレーム発火)を設定して初期化する。
	void Setup(const Vector3& center, float radius, float tickInterval = 0.0f);

	// AreaEffectManager から毎フレーム呼ばれ、全対象を円判定に掛ける。
	void Drive(const std::vector<Target>& targets, float deltaTime);

	// デバッグ描画（円の輪郭）
	void Draw(Line* line);

	///--- アクセッサ ---///

	void        SetCenter(const Vector3& c) { if (area_) area_->SetCenter(c); }
	Vector3     GetCenter() const           { return area_ ? area_->GetCenter() : Vector3(); }
	void        SetRadius(float r)          { if (area_) area_->SetRadius(r); }
	void        SetActive(bool a)           { if (area_) area_->SetActive(a); }
	bool        IsActive() const            { return area_ && area_->IsActive(); }
	CircleArea* GetArea()                   { return area_.get(); }

protected:
	///--- 派生クラスが効果を実装するフック（既定は空） ---///

	virtual void OnEnter([[maybe_unused]] IAreaEffectTarget* target) {}
	virtual void OnStay ([[maybe_unused]] IAreaEffectTarget* target) {}
	virtual void OnExit ([[maybe_unused]] IAreaEffectTarget* target) {}

protected:
	std::shared_ptr<CircleArea> area_;
};

#pragma once

// Engine
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Systems/Camera/Camera.h"
#include "Collision/AABB/AABBCollider.h"

#include <memory>

// ============================================================
// 地面クラス
// ============================================================
class Ground
{
public:
	// ============================================================
	// 基本関数
	// ============================================================

	// 初期化
	void Initialize(YoRigine::Camera* camera);

	// 更新
	void Update();

	// 描画
	void Draw();

public:
	// ============================================================
	// アクセッサ
	// ============================================================

	// 色変更
	Vector4& GetColor() { return obj_->GetColor(); }

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// 描画する3Dオブジェクトのスマートポインタ
	std::unique_ptr<YoRigine::Object3d> obj_;

	// 描画に使用するカメラポインタ
	YoRigine::Camera* camera_;

	// 地面のワールド座標、回転、スケールを管理するトランスフォーム
	YoRigine::WorldTransform wt_;

	// スタンプ配置等の Raycast 受け用 AABB (narrow-phase 衝突には参加しない)
	std::shared_ptr<AABBCollider> collider_;
};
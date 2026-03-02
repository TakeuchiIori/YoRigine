#pragma once

// Engine
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Systems/Camera/Camera.h"

/// <summary>
/// 地面クラス
/// </summary>
class Ground
{
public:
	// 初期化
	void Initialize(Camera* camera);

	// 更新
	void Update();

	// 描画
	void Draw();

public:
	// 色変更
	Vector4& GetColor() { return obj_->GetColor(); }
private:
	// ポインタ
	std::unique_ptr<Object3d> obj_;
	Camera* camera_;
	// ワールドトランスフォーム
	WorldTransform wt_;
};


#pragma once

// Engine
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Systems/Camera/Camera.h"

class Ground
{
public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Camera* camera);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();


private:
	// ポインタ
	std::unique_ptr<Object3d> obj_;
	Camera* camera_;
	// ワールドトランスフォーム
	WorldTransform worldTransform_;
};


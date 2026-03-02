#include "Ground.h"

/// <summary>
/// 地面オブジェクトを初期化
/// </summary>
/// <param name="camera">描画に使用するカメラ</param>
void Ground::Initialize(Camera* camera)
{
	camera_ = camera;
	// Object3dの生成
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("Ground.obj");

	// ワールドトランスフォーム初期化
	wt_.Initialize();
	//wt_.translate_.y -= 2.0f;
}

/// <summary>
/// 地面オブジェクトの更新処理
/// </summary>
void Ground::Update()
{
	wt_.UpdateMatrix();
}

/// <summary>
/// 地面オブジェクトの描画処理
/// </summary>
void Ground::Draw()
{
	obj_->Draw(camera_, wt_);
}

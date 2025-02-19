#include "Ground.h"

void Ground::Initialize(Camera* camera)
{
	camera_ = camera;
	// Object3dの生成
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("terrain.obj");

	worldTransform_.Initialize();
	worldTransform_.translation_.y -= 2.0f;
}

void Ground::Update()
{
	worldTransform_.UpdateMatrix();
}

void Ground::Draw()
{
	obj_->Draw(camera_,worldTransform_);
}

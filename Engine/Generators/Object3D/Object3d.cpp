#include "Object3d.h"
#include "Object3dCommon.h"
#include <assert.h>
// C++
#include <fstream>
#include <sstream>
#include <algorithm>

// Engine
#include "Loaders./Texture./TextureManager.h"
#include "Loaders./Model/ModelManager.h"
#include "Loaders./Model/Model.h"
#include "WorldTransform./WorldTransform.h"


#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG

void Object3d::Initialize()
{
	// 引数で受け取ってメンバ変数に記録する
	this->object3dCommon_ = Object3dCommon::GetInstance();

	CreateMaterialResource();

	CreateCameraResource();
}
void Object3d::UpdateAnimation()
{
	// アニメーションの更新
	model_->UpdateAnimation();
}


void Object3d::Draw(Camera* camera,WorldTransform& worldTransform)
{

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 worldMatrix;
	if (model_) {
		if (camera) {
			const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();

			// 
			if (!model_->GetModelData().hasBones) {
				worldViewProjectionMatrix = worldTransform.GetMatWorld() * model_->GetModelData().rootNode.localMatrix * viewProjectionMatrix;
				worldMatrix = worldTransform.GetMatWorld() * model_->GetModelData().rootNode.localMatrix;
			}
			else {
				worldViewProjectionMatrix = worldTransform.GetMatWorld() * viewProjectionMatrix;
				worldMatrix = worldTransform.GetMatWorld();
			}
		}
		else {

			worldViewProjectionMatrix = worldTransform.GetMatWorld();
			worldMatrix = worldTransform.GetMatWorld(); // 初期化が必要
		}
	}

	worldTransform.SetMapWVP(worldViewProjectionMatrix);
	worldTransform.SetMapWorld(worldMatrix);

	// マテリアル
	object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// TransformatonMatrixCB
	object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, worldTransform.GetConstBuffer()->GetGPUVirtualAddress());
	// カメラ
	object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}
}

void Object3d::DrawSkeleton(const Model::Skeleton& skeleton, Line& line)
{
	model_->DrawSkeleton(model_->GetSkeleton(),line);
}

void Object3d::CreateMaterialResource()
{
	materialResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = true;
	materialData_->shininess = 30.0f;
	materialData_->uvTransform = MakeIdentity4x4();
}

void Object3d::CreateCameraResource()
{
	cameraResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
}




void Object3d::SetModel(const std::string& filePath, bool isAnimation)
{
	// 拡張子を取り除く処理
	std::string basePath = filePath;
	std::string fileName;
	if (basePath.size() > 4) {
		// .obj または .gltf の場合に削除
		if (basePath.substr(basePath.size() - 4) == ".obj") {
			basePath = basePath.substr(0, basePath.size() - 4);
			fileName = basePath + ".obj";
		}
		else if (basePath.size() > 5 && basePath.substr(basePath.size() - 5) == ".gltf") {
			basePath = basePath.substr(0, basePath.size() - 5);
			fileName = basePath + ".gltf";
		}
	}

	// .obj 読み込み (第一引数には拡張子なしのパス)
	ModelManager::GetInstance()->LoadModel("Resources./Models./" + basePath, fileName, isAnimation);

	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(fileName);

}

void Object3d::MaterialByImGui()
{
#ifdef _DEBUG
	ImGui::Begin("Material");
	Vector4 materialColor = GetMaterialColor();
	if (ImGui::ColorEdit4("Material Color", &materialColor.x)) {
		SetMaterialColor(materialColor);
	}

	float shininess = GetMaterialShininess();
	if (ImGui::SliderFloat("Shininess", &shininess, 0.1f, 200.0f, "%.2f")) {
		SetMaterialShininess(shininess);
	}

	Matrix4x4 uvTransform = GetMaterialUVTransform();
	if (ImGui::InputFloat("UV Scale X", &uvTransform.m[0][0], 0.1f, 1.0f, "%.2f")) {
		uvTransform.m[0][0] = std::clamp(uvTransform.m[0][0], 0.1f, 10.0f);
		SetMaterialUVTransform(uvTransform);
	}
	if (ImGui::InputFloat("UV Scale Y", &uvTransform.m[1][1], 0.1f, 1.0f, "%.2f")) {
		uvTransform.m[1][1] = std::clamp(uvTransform.m[1][1], 0.1f, 10.0f);
		SetMaterialUVTransform(uvTransform);
	}

	bool isMaterialLight = IsMaterialEnabled();
	if (ImGui::Checkbox("Use Lighting", &isMaterialLight)) {
		SetMaterialEnabled(isMaterialLight);
	}
	ImGui::End();
#endif // _DEBUG
}


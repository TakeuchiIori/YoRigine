#include "SkyBox.h"
#include "CubeMap/CubeMap.h"

// ============================================================
// 初期化
// ============================================================
void SkyBox::Initialize(YoRigine::Camera* camera, const std::string& textureFilePath) {
	//------------------------------------------------------------
	// キューブマップ生成と初期化
	//------------------------------------------------------------
	cubeMap_ = std::make_unique<YoRigine::CubeMap>();
	cubeMap_->Initialize(camera, textureFilePath);

	// JSON設定初期化
	InitJson();
}

// ============================================================
// 更新処理
// ============================================================
void SkyBox::Update() {
	// キューブマップの更新（回転やカメラ追従など）
	if (cubeMap_) {
		cubeMap_->Update();
	}
}

// ============================================================
// 描画処理
// ============================================================
void SkyBox::Draw() {
	// キューブマップ描画
	if (cubeMap_) {
		cubeMap_->Draw();
	}
}

// ============================================================
// テクスチャ変更
// ============================================================
void SkyBox::SetTextureFilePath(const std::string& filePath) {
	if (cubeMap_) {
		cubeMap_->SetTextureFilePath(filePath);
	}
}

// ============================================================
// Json初期化
// ============================================================
void SkyBox::InitJson() {
	//------------------------------------------------------------
	// YoRigine::JsonManager 設定（位置・回転・スケールを調整可能に）
	//------------------------------------------------------------
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("SkyBox", "Resources/Json/CubeMap/SkyBox");
	jsonManager_->SetCategory("CubeMap");
	jsonManager_->SetSubCategory("SkyBox");

	jsonManager_->Register("Translate", &cubeMap_->wt_.translate_);
	jsonManager_->Register("Rotate", &cubeMap_->wt_.rotate_);
	jsonManager_->Register("Scale", &cubeMap_->wt_.scale_);
}

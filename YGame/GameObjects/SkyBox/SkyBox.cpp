#include "SkyBox.h"
#include "CubeMap/CubeMap.h"

/// <summary>
/// スカイボックス初期化
/// </summary>
/// <param name="camera">使用するカメラ</param>
/// <param name="textureFilePath">読み込むキューブマップテクスチャのパス</param>
void SkyBox::Initialize(Camera* camera, const std::string& textureFilePath) {
	//------------------------------------------------------------
	// キューブマップ生成と初期化
	//------------------------------------------------------------
	cubeMap_ = std::make_unique<YoRigine::CubeMap>();
	cubeMap_->Initialize(camera, textureFilePath);

	// JSON設定初期化
	InitJson();
}

/// <summary>
/// 更新処理
/// </summary>
void SkyBox::Update() {
	// キューブマップの更新（回転やカメラ追従など）
	if (cubeMap_) {
		cubeMap_->Update();
	}
}

/// <summary>
/// 描画処理
/// </summary>
void SkyBox::Draw() {
	// キューブマップ描画
	if (cubeMap_) {
		cubeMap_->Draw();
	}
}

/// <summary>
/// テクスチャの差し替え
/// </summary>
/// <param name="filePath">新しいテクスチャのパス</param>
void SkyBox::SetTextureFilePath(const std::string& filePath) {
	if (cubeMap_) {
		cubeMap_->SetTextureFilePath(filePath);
	}
}

/// <summary>
/// JSON登録処理
/// </summary>
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

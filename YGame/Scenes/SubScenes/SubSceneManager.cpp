#include "SubSceneManager.h"
#include "OffScreen/PostEffectManager.h"
#include "Systems/GameTime/GameTime.h"
#include "BattleScene.h"
#include "FieldScene.h"
#include <cmath>

/// <summary>
/// 初期化処理
/// </summary>
void SubSceneManager::Initialize(Camera* camera, Player* player) {
	camera_ = camera;
	player_ = player;
	state_ = SubSceneState::IDLE;

	auto postEffectManager = PostEffectManager::GetInstance();

	//------------------------------------------------------------
	// シーン遷移用ポストエフェクトの追加
	//------------------------------------------------------------
	shatterEffectIndex_ = postEffectManager->AddEffect(
		OffScreen::OffScreenEffectType::ShatterTransition,
		"SceneTransition"
	);

	radialBlurEffectIndex_ = postEffectManager->AddEffect(
		OffScreen::OffScreenEffectType::RadialBlur,
		"SceneTransitionBlur"
	);

	postEffectManager->SetEffectEnabled(shatterEffectIndex_, false);
	postEffectManager->SetEffectEnabled(radialBlurEffectIndex_, false);
}

/// <summary>
/// 終了処理
/// </summary>
void SubSceneManager::Finalize() {
	if (pendingTransitionData_) {
		delete pendingTransitionData_;
		pendingTransitionData_ = nullptr;
	}

	for (auto& [name, scene] : subScenes_) {
		if (scene) scene->Finalize();
	}
	subScenes_.clear();
	currentScene_ = nullptr;
}

/// <summary>
/// 即時シーン切り替え（フェードなし）
/// </summary>
/// <param name="sceneName"></param>
void SubSceneManager::SwitchToScene(const std::string& sceneName) {
	if (state_ == SubSceneState::TRANSITIONING) return;

	auto it = subScenes_.find(sceneName);
	if (it == subScenes_.end()) return;

	DeactivateCurrentScene();
	ActivateScene(sceneName);
}

/// <summary>
/// フェード付きシーン切り替え
/// </summary>
void SubSceneManager::SwitchToSceneWithFade(const std::string& sceneName) {
	if (state_ == SubSceneState::TRANSITIONING) return;

	auto it = subScenes_.find(sceneName);
	if (it == subScenes_.end()) return;

	state_ = SubSceneState::TRANSITIONING;
	pendingSceneName_ = sceneName;
	transitionTime_ = 0.0f;
	hasSceneSwitched_ = false;

	auto postEffectManager = PostEffectManager::GetInstance();

	//------------------------------------------------------------
	// エフェクト初期化
	//------------------------------------------------------------
	postEffectManager->SetEffectEnabled(shatterEffectIndex_, true);
	postEffectManager->SetEffectEnabled(radialBlurEffectIndex_, true);

	OffScreen::ShatterTransitionParams params{};
	params.progress = 0.0f;
	params.resolution = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };
	params.time = 0.0f;

	auto shatterData = postEffectManager->GetEffectData(shatterEffectIndex_);
	if (shatterData) {
		shatterData->params.shatter = params;
	}

	auto blurData = postEffectManager->GetEffectData(radialBlurEffectIndex_);
	if (blurData) {
		auto& blur = blurData->params.radialBlur;
		blur.center = { 0.5f, 0.5f };
		blur.width = 0.0f;
		blur.sampleCount = 3;
		blur.isRadial = true;
	}
}

/// <summary>
/// 更新処理
/// </summary>
void SubSceneManager::Update() {
	if (state_ == SubSceneState::TRANSITIONING) {
		UpdateTransition();
		return;
	}

	if (currentScene_ && !currentScene_->IsPaused()) {
		currentScene_->Update();
	}
}

/// <summary>
/// シーン遷移演出の更新
/// </summary>
void SubSceneManager::UpdateTransition() {
	auto postEffectManager = PostEffectManager::GetInstance();
	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	transitionTime_ += deltaTime;
	float progress = std::min(transitionTime_ / transitionDuration_, 1.0f);

	//------------------------------------------------------------
	// 破片エフェクト更新
	//------------------------------------------------------------
	auto shatterData = postEffectManager->GetEffectData(shatterEffectIndex_);
	if (shatterData) {
		shatterData->params.shatter.progress = progress;
		shatterData->params.shatter.resolution = { (float)WinApp::kClientWidth, (float)WinApp::kClientHeight };
		shatterData->params.shatter.time = transitionTime_;
	}

	//------------------------------------------------------------
	// ブラー演出更新
	//------------------------------------------------------------
	auto blurData = postEffectManager->GetEffectData(radialBlurEffectIndex_);
	if (blurData) {
		float strength = sinf(progress * 3.1415926f);
		auto& blur = blurData->params.radialBlur;
		blur.center = { 0.5f, 0.5f };
		blur.width = 0.02f * strength;
		blur.sampleCount = 8 + int(16 * strength);
		blur.isRadial = true;
	}

	//------------------------------------------------------------
	// シーン切り替えタイミング
	//------------------------------------------------------------
	if (!hasSceneSwitched_ && progress >= switchThreshold_) {
		DeactivateCurrentScene();
		ActivateScene(pendingSceneName_);
		ApplyTransitionData();
		hasSceneSwitched_ = true;
	}

	//------------------------------------------------------------
	// 遷移完了処理
	//------------------------------------------------------------
	if (progress >= 1.0f) {
		postEffectManager->SetEffectEnabled(shatterEffectIndex_, false);
		postEffectManager->SetEffectEnabled(radialBlurEffectIndex_, false);

		state_ = SubSceneState::ACTIVE;
		hasSceneSwitched_ = false;
		transitionTime_ = 0.0f;

		if (currentScene_) currentScene_->OnResume();

		if (pendingTransitionData_) {
			delete pendingTransitionData_;
			pendingTransitionData_ = nullptr;
		}
	}
}

/// <summary>
/// 3Dオブジェクト描画
/// </summary>
void SubSceneManager::DrawObject() {
	if (currentScene_ && currentScene_->IsActive()) {
		currentScene_->DrawObject();
	}
}

/// <summary>
/// ライン描画（デバッグ用）
/// </summary>
void SubSceneManager::DrawLine() {
	if (currentScene_ && currentScene_->IsActive()) {
		currentScene_->DrawLine();
	}
}

/// <summary>
/// UI描画
/// </summary>
void SubSceneManager::DrawUI() {
	if (currentScene_ && currentScene_->IsActive()) {
		currentScene_->DrawUI();
	}
}

void SubSceneManager::DrawNonOffscreen()
{
	if (currentScene_ && currentScene_->IsActive()) {
		currentScene_->DrawNonOffscreen();
	}
}

void SubSceneManager::DrawShadow()
{
	if (currentScene_ && currentScene_->IsActive()) {
		currentScene_->DrawShadow();
	}
}

/// <summary>
/// 遷移リクエストの受付処理
/// </summary>
void SubSceneManager::HandleTransitionRequest(const SubSceneTransitionRequest& request) {
	if (pendingTransitionData_) {
		delete pendingTransitionData_;
		pendingTransitionData_ = nullptr;
	}

	pendingTransitionData_ = request.transitionData;
	pendingTransitionType_ = request.type;

	switch (request.type) {
	case SubSceneTransitionType::TO_FIELD:
		SwitchToSceneWithFade("Field");
		break;
	case SubSceneTransitionType::TO_BATTLE:
		SwitchToSceneWithFade("Battle");
		break;
	case SubSceneTransitionType::TO_MENU:
		SwitchToSceneWithFade("Menu");
		break;
	case SubSceneTransitionType::CUSTOM:
		if (!request.targetSceneName.empty()) {
			SwitchToSceneWithFade(request.targetSceneName);
		}
		break;
	}
}

/// <summary>
/// 指定シーンの取得
/// </summary>
BaseSubScene* SubSceneManager::GetScene(const std::string& name) const {
	auto it = subScenes_.find(name);
	if (it != subScenes_.end()) {
		return it->second.get();
	}
	return nullptr;
}

/// <summary>
/// シーン遷移データの適用（バトル／フィールド間）
/// </summary>
void SubSceneManager::ApplyTransitionData() {
	if (!pendingTransitionData_ || !currentScene_) return;

	if (pendingTransitionType_ == SubSceneTransitionType::TO_BATTLE) {
		auto* battleScene = dynamic_cast<BattleScene*>(currentScene_);
		if (battleScene) {
			//auto* data = static_cast<BattleTransitionData*>(pendingTransitionData_);
			////battleScene->StartBattle(*data);
		}
	} else if (pendingTransitionType_ == SubSceneTransitionType::TO_FIELD) {
		auto* fieldScene = dynamic_cast<FieldScene*>(currentScene_);
		if (fieldScene) {
			auto* data = static_cast<FieldReturnData*>(pendingTransitionData_);
			fieldScene->HandleBattleReturn(*data);
		}
	}
}

/// <summary>
/// シーンの有効化
/// </summary>
void SubSceneManager::ActivateScene(const std::string& sceneName) {
	auto it = subScenes_.find(sceneName);
	if (it != subScenes_.end()) {
		currentScene_ = it->second.get();
		currentScene_->OnEnter();

		if (state_ == SubSceneState::TRANSITIONING) {
			currentScene_->OnPause();
		} else {
			state_ = SubSceneState::ACTIVE;
		}
	}
}

/// <summary>
/// 現在のシーンを無効化
/// </summary>
void SubSceneManager::DeactivateCurrentScene() {
	if (currentScene_) {
		currentScene_->OnExit();
		currentScene_ = nullptr;
	}
}

/// <summary>
/// カメラモード設定
/// </summary>
void SubSceneManager::SetCameraMode(CameraMode mode) {
	if (currentScene_) {
		currentScene_->SetCameraMode(mode);
	}
}

/// <summary>
/// 現在のカメラモード取得
/// </summary>
CameraMode SubSceneManager::GetCameraMode() const {
	if (currentScene_) {
		return currentScene_->GetCameraMode();
	}
	return CameraMode::DEFAULT;
}

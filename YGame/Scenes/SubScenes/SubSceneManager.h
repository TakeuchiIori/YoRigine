#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

#include "BaseSubScene.h"
#include "SceneDataStructures.h"

///************************* サブシーン状態定義 *************************///
enum class SubSceneState {
	IDLE,
	ACTIVE,
	TRANSITIONING,
};

///************************* サブシーン管理クラス *************************///
class SubSceneManager {
public:
	///************************* 基本関数 *************************///

	// コンストラクタ
	SubSceneManager() = default;

	// デストラクタ
	~SubSceneManager() = default;

	// 初期化処理
	void Initialize(Camera* camera, Player* player);

	// 終了処理
	void Finalize();

	///************************* サブシーン登録 *************************///

	// サブシーンを登録
	template<typename T>
	void RegisterSubScene(const std::string& name, std::unique_ptr<T> scene) {
		static_assert(std::is_base_of<BaseSubScene, T>::value, "T must derive from BaseSubScene");
		scene->SetTransitionCallback([this](const SubSceneTransitionRequest& request) {
			HandleTransitionRequest(request);
			});
		subScenes_[name] = std::move(scene);
	}

	///************************* シーン切り替え *************************///

	// 即時でシーンを切り替える
	void SwitchToScene(const std::string& sceneName);

	// フェード付きでシーンを切り替える
	void SwitchToSceneWithFade(const std::string& sceneName);

	///************************* 更新・描画 *************************///

	// 更新処理
	void Update();

	// オブジェクト描画処理
	void DrawObject();

	// ライン描画処理
	void DrawLine();

	// UI描画処理
	void DrawUI();

	// 非オフスクリーン描画処理
	void DrawNonOffscreen();

	// シャドウ描画処理
	void DrawShadow();

	///************************* 遷移処理 *************************///

	// シーン遷移リクエストを処理
	void HandleTransitionRequest(const SubSceneTransitionRequest& request);

	///************************* アクセッサ *************************///

	// 現在のシーンを取得
	BaseSubScene* GetCurrentScene() const { return currentScene_; }

	// 指定名のシーンを取得
	BaseSubScene* GetScene(const std::string& name) const;

	// 現在のシーン名を取得
	std::string GetCurrentSceneName() const {
		return currentScene_ ? currentScene_->GetSceneName() : "";
	}

	// 遷移中か確認
	bool IsTransitioning() const { return state_ == SubSceneState::TRANSITIONING; }

	// カメラモードを設定
	void SetCameraMode(CameraMode mode);

	// カメラモードを取得
	CameraMode GetCameraMode() const;

	// 遷移時間を設定
	void SetTransitionDuration(float duration) { transitionDuration_ = duration; }

	// シーン切り替えしきい値を設定
	void SetSwitchThreshold(float threshold) { switchThreshold_ = threshold; }

private:
	///************************* 内部処理 *************************///

	// シーンを有効化
	void ActivateScene(const std::string& sceneName);

	// 現在のシーンを無効化
	void DeactivateCurrentScene();

	// 遷移中の更新処理
	void UpdateTransition();

	// 遷移データを適用
	void ApplyTransitionData();

private:
	///************************* メンバ変数 *************************///

	std::unordered_map<std::string, std::unique_ptr<BaseSubScene>> subScenes_;
	BaseSubScene* currentScene_ = nullptr;
	SubSceneState state_ = SubSceneState::IDLE;

	int shatterEffectIndex_ = -1;
	int radialBlurEffectIndex_ = -1;

	std::string pendingSceneName_;
	float transitionTime_ = 0.0f;
	float transitionDuration_ = 1.0f;
	float switchThreshold_ = 0.6f;
	bool hasSceneSwitched_ = false;

	void* pendingTransitionData_ = nullptr;
	SubSceneTransitionType pendingTransitionType_;

	Camera* camera_ = nullptr;
	Player* player_ = nullptr;
};

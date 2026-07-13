#pragma once

// C++
#include <memory>
#include <mutex>

// Engine
#include "AbstractSceneFactory.h"

// MainScenes
#include "Transitions/Fade/Fade.h"
#include "Transitions/Base/TransitionFactory.h"

class PostEffectManager;
class BaseObjectManager;

/// <summary>
/// メインシーンの管理クラス
/// </summary>
class SceneManager {
public:
	///************************* 基本関数 *************************///
	// シングルトン
	static SceneManager* GetInstance();
	SceneManager() = default;
	~SceneManager() = default;

	void Initialize();
	void Update();
	void PerformSceneTransition();
	void Draw();
	// オフスクリーン描画を行わない描画
	void DrawNonOffscreen();
	// 影描画
	void DrawShadow();

	void Finalize();
	// シーン変更
	void ChangeScene(const std::string& sceneName);
	void ChangeSceneImmediate(const std::string& sceneName);

public:
	///************************* アクセッサ *************************///
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }
	void SetTransitionFactory(std::unique_ptr<TransitionFactory> factory) { transitionFactory_ = std::move(factory); }

	// ============================================================
	// 依存先マネージャの注入 (DI)
	//   所有はしない (借用のみ)。Finalize() で nullptr に戻すのでダングリングポインタは残らない。
	// ============================================================
	void SetPostEffectManager(PostEffectManager* postEffectManager) { postEffectManager_ = postEffectManager; }
	void SetBaseObjectManager(BaseObjectManager* baseObjectManager) { baseObjectManager_ = baseObjectManager; }
	BaseScene* GetScene() const { return scene_.get(); }
	std::string GetCurrentSceneName() const {
		if (scene_) {
			return scene_->GetName();
		}
		return "";
	}

private:
	///************************* メンバ変数 *************************///
	static std::unique_ptr<SceneManager> instance;
	static std::once_flag initInstanceFlag;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	// 現在のシーン
	std::unique_ptr<BaseScene> scene_ = nullptr;
	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_ = nullptr;

	// シーンファクトリー（借りてくる）
	AbstractSceneFactory* sceneFactory_ = nullptr;

	// トランジション関連
	enum class TransitionState {
		None,       // トランジションなし
		FadeOut,    // フェードアウト中（画面が暗くなる）
		FadeIn      // フェードイン中（画面が明るくなる）
	};

	std::unique_ptr<TransitionFactory> transitionFactory_;
	std::unique_ptr<ISceneTransition> transition_;
	TransitionState transitionState_ = TransitionState::None;

	// 依存先マネージャ (借用のみ・非所有)。Initialize() 前に Set 系で注入すること。
	PostEffectManager* postEffectManager_ = nullptr;
	BaseObjectManager* baseObjectManager_ = nullptr;
};
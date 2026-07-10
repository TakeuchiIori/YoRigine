#include "SceneManager.h"
#include "Sprite/SpriteCommon.h"
#include "OffScreen/PostEffectManager.h"
#include <Object3D/BaseObjectManager.h>
#include <assert.h>

std::unique_ptr<SceneManager> SceneManager::instance = nullptr;
std::once_flag SceneManager::initInstanceFlag;

/// <summary>
/// シングルトンインスタンス取得
/// </summary>
SceneManager* SceneManager::GetInstance() {
	std::call_once(initInstanceFlag, []() {
		instance = std::make_unique<SceneManager>();
		});
	return instance.get();
}

/// <summary>
/// 終了処理
/// </summary>
void SceneManager::Finalize() {
	// 現在のシーンが存在する場合、終了して解放
	if (scene_) {
		scene_->Finalize();
		scene_ = nullptr;
	}
}

/// <summary>
/// 初期化処理
/// </summary>
void SceneManager::Initialize() {
	// トランジションが設定されている場合のみ初期化
	if (transitionFactory_) {
		transition_ = transitionFactory_->CreateTransition();
		transition_->Initialize();
	}
}

/// <summary>
/// 更新処理
/// </summary>
void SceneManager::Update() {
	//------------------------------------------------------------
	// 現在のシーン更新
	//------------------------------------------------------------
	if (scene_) {
		scene_->Update();
	}

	//------------------------------------------------------------
	// トランジション更新と状態管理
	//------------------------------------------------------------
	if (transition_) {
		transition_->Update();

		switch (transitionState_) {
		case TransitionState::FadeOut:
			// フェードアウト完了時にシーン切り替え
			if (transition_->IsFinished()) {

				PerformSceneTransition();

				// フェードイン開始
				transition_->StartTransition();
				transitionState_ = TransitionState::FadeIn;
			}
			break;

		case TransitionState::FadeIn:
			// フェードイン完了後、通常状態に戻す
			if (transition_->IsFinished()) {
				transitionState_ = TransitionState::None;
			}
			break;

		case TransitionState::None:
			// 次のシーン予約があればフェードアウト開始
			if (nextScene_) {
				transition_->EndTransition();
				transitionState_ = TransitionState::FadeOut;
			}
			break;
		}
	}
}

void SceneManager::PerformSceneTransition() {

	//------------------------------------------------------------
	// 現在のシーンを安全に終了
	//------------------------------------------------------------
	if (scene_) {
		scene_->Finalize();
		scene_ = nullptr;
	}

	//------------------------------------------------------------
	// 新しいシーンに切り替え
	//------------------------------------------------------------

	// ポストエフェクト初期化
	PostEffectManager::GetInstance()->Reset(); 
	scene_ = std::move(nextScene_);
	nextScene_ = nullptr;
	if (scene_) {
		// 前シーンの BaseObject 登録を一掃してから次シーンを初期化する。
		// これで各シーン側に「登録解除」コードを書く必要がなくなる。
		BaseObjectManager::GetInstance()->ClearAll();
		scene_->SetSceneManager(this);
		scene_->Initialize();
	}
}

/// <summary>
/// シーン描画（オフスクリーン対象）
/// </summary>
void SceneManager::Draw() {
	if (scene_) {
		scene_->Draw();
	}
}

/// <summary>
/// シーン描画（オフスクリーン外 / トランジション描画含む）
/// </summary>
void SceneManager::DrawNonOffscreen() {
	//------------------------------------------------------------
	// 通常シーン描画
	//------------------------------------------------------------
	if (scene_) {
		scene_->DrawNonOffscreen();
	}

	//------------------------------------------------------------
	// トランジション描画
	//------------------------------------------------------------
	if (transition_ && transitionState_ != TransitionState::None) {
		transition_->Draw();
	}
}

void SceneManager::DrawShadow() {
	if (scene_) {
		scene_->DrawShadow();
	}
}

/// <summary>
/// シーン変更要求
/// </summary>
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_);
	// 既に遷移中、または次シーン予約済みなら無視
	if (transitionState_ != TransitionState::None || nextScene_) {
		return;
	}

	nextScene_ = sceneFactory_->CreateScene(sceneName);
	//------------------------------------------------------------
	// 初回シーン生成処理
	//------------------------------------------------------------
	if (!scene_) {
		scene_ = std::move(nextScene_);
		nextScene_ = nullptr;
		// 初回シーンも空の状態から始められるように一掃しておく。
		BaseObjectManager::GetInstance()->ClearAll();
		scene_->SetSceneManager(this);
		scene_->Initialize();

		// 初回のみフェードイン開始
		if (transition_) {
			transition_->StartTransition();
			transitionState_ = TransitionState::FadeIn;
		}
	}
}

/// <summary>
///	瞬時にシーン遷移（フェードなし）
/// </summary>
/// <param name="sceneName"></param>
void SceneManager::ChangeSceneImmediate(const std::string& sceneName) {
	assert(sceneFactory_);

	// ポストエフェクトのリセット
	PostEffectManager::GetInstance()->Reset();
	// 現在のシーンを終了
	if (scene_) {
		scene_->Finalize();
		scene_ = nullptr;
	}

	// 前シーンの BaseObject 登録を一掃する（フェード遷移の PerformSceneTransition と同様）。
	// これを忘れると、解放済みオブジェクトへのダングリング参照が entries_ に残り、
	// 次シーンの DrawShadowAll / UpdateAll 等で落ちる（Develop への即時遷移で発生）。
	BaseObjectManager::GetInstance()->ClearAll();

	// 新しいシーンを生成して初期化
	scene_ = sceneFactory_->CreateScene(sceneName);
	scene_->SetSceneManager(this);
	scene_->Initialize();

	// トランジションの状態をリセット
	transitionState_ = TransitionState::None;
}

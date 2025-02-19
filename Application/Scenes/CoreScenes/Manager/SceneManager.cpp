#include "SceneManager.h"
#include "Sprite/SpriteCommon.h"
#include <assert.h>

std::unique_ptr<SceneManager> SceneManager::instance = nullptr;
std::once_flag SceneManager::initInstanceFlag;
SceneManager* SceneManager::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance = std::make_unique<SceneManager>();
		});
	return instance.get();
}

void SceneManager::Finalize()
{
	// 最後のシーンの終了と解放
	scene_->Finalize();
	delete scene_;
}

void SceneManager::Initialize()
{
    if (transitionFactory_) {
		transition_ = transitionFactory_->CreateTransition();
		transition_->Initialize();
    }
}

void SceneManager::Update()
{
    // 現在のシーンの更新
    if (scene_) {
        scene_->Update();
    }

    // トランジションの更新
    if (transition_) {
        transition_->Update();

        switch (transitionState_) {
        case TransitionState::TransitionOut:
            if (transition_->IsFinished()) {
                // シーン切り替え
                if (scene_) {
                    scene_->Finalize();
                    delete scene_;
                }
                scene_ = nextScene_;
                nextScene_ = nullptr;
                scene_->SetSceneManager(this);
                scene_->Initialize();

                transition_->StartTransition();
                transitionState_ = TransitionState::TransitionIn;
            }
            break;

        case TransitionState::TransitionIn:
            if (transition_->IsFinished()) {
                transitionState_ = TransitionState::None;
            }
            break;

        case TransitionState::None:
            if (nextScene_) {
                transition_->EndTransition();
                transitionState_ = TransitionState::TransitionOut;
            }
            break;
        }
    }
}

void SceneManager::Draw()
{
    // 現在のシーンの描画
    if (scene_) {
        scene_->Draw();
    }

    // 遷移の描画
    if (transition_ && transitionState_ != TransitionState::None) {
        transition_->Draw();
    }
}

void SceneManager::ChangeScene(const std::string& sceneName)
{
    assert(sceneFactory_);

    if (transitionState_ != TransitionState::None || nextScene_) {
        return;
    }

    nextScene_ = sceneFactory_->CreateScene(sceneName);

    // 初回シーン生成時
    if (!scene_) {
        scene_ = nextScene_;
        nextScene_ = nullptr;
        scene_->SetSceneManager(this);
        scene_->Initialize();
        if (transition_) {
            transition_->StartTransition();
            transitionState_ = TransitionState::TransitionIn;
        }
    }
}


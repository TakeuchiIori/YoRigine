#pragma once

// C++
#include <memory>
#include <mutex>

// Engine
#include "CoreScenes/Base/BaseScene.h"
#include "CoreScenes/Factory/AbstractSceneFactory.h"

// MainScenes
#include <MainScenes/Transitions/Fade/Fade.h>
#include <MainScenes/Transitions/Base/TransitionFactory.h>

class SceneManager {
public:
    // シングルトン
    static SceneManager* GetInstance();
    SceneManager() = default;
    ~SceneManager() = default;

    /// <summary> 初期化 </summary>
    void Initialize();

    /// <summary> 更新 </summary>
    void Update();

    /// <summary> 描画 </summary>
    void Draw();

    /// <summary> 次シーン予約 </summary>
    /// <param name="sceneName"></param>
    void ChangeScene(const std::string& sceneName);

    /// <summary> 後処理（シングルトンの破棄）</summary>
    void Finalize();

    // アクセッサ
    void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }
    void SetTransitionFactory(std::unique_ptr<TransitionFactory> factory) { transitionFactory_ = std::move(factory); }

private:
    // シングルトン
    static std::unique_ptr<SceneManager> instance;
    static std::once_flag initInstanceFlag;


    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // 現在のシーン
    // 次のシーン
    BaseScene* scene_ = nullptr;
    BaseScene* nextScene_ = nullptr;

    // シーンファクトリー（借りてくる）
    AbstractSceneFactory* sceneFactory_ = nullptr;

    // トランジション関連
    enum class TransitionState {
        None,
        TransitionOut,
        TransitionIn
    };

    std::unique_ptr<TransitionFactory> transitionFactory_;
    std::unique_ptr<ISceneTransition> transition_;
    TransitionState transitionState_ = TransitionState::None;
};

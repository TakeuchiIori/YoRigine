#pragma once

// C++
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Engine
#include "SceneSystems/Transitions/Base/ISceneTransition.h"

// App
#include "BaseSubScene.h"
#include "SceneDataStructures.h"

///************************* サブシーン管理クラス *************************///
/// GameScene 内のサブシーンを保持し、遷移演出付きで切り替える。
/// 個々のサブシーンの型や中身は知らない（受け渡しデータは OnEnter
/// に渡すだけ）。
class SubSceneManager {
public:
  using EventHandler = std::function<void(SubSceneEvent)>;

  ///************************* 基本関数 *************************///

  // 初期化処理（遷移演出の準備）
  void Initialize();

  // 終了処理
  void Finalize();

  ///************************* サブシーン登録 *************************///

  // サブシーンを登録
  template <typename T>
  void RegisterSubScene(const std::string &name, std::unique_ptr<T> scene) {
    static_assert(std::is_base_of<BaseSubScene, T>::value,
                  "T must derive from BaseSubScene");
    scene->SetTransitionCallback(
        [this](const SubSceneTransitionRequest &request) {
          SwitchToSceneWithTransition(request.targetSceneName, request.payload);
        });
    scene->SetEventCallback([this](SubSceneEvent event) {
      if (eventHandler_) {
        eventHandler_(event);
      }
    });
    subScenes_[name] = std::move(scene);
  }

  // サブシーンからの通知を受け取る関数を設定（GameScene が受け取る）
  void SetEventHandler(EventHandler handler) {
    eventHandler_ = std::move(handler);
  }

  ///************************* シーン切り替え *************************///

  // 即時でシーンを切り替える
  void SwitchToScene(const std::string &sceneName,
                     const SubScenePayload &payload = {});

  // 遷移演出付きでシーンを切り替える。遷移中などで受け付けなかった場合は false
  bool SwitchToSceneWithTransition(const std::string &sceneName,
                                   const SubScenePayload &payload = {});

  ///************************* 更新・描画 *************************///

  void Update();
  void DrawObject();
  void DrawLine();
  void DrawUI();
  void DrawNonOffscreen();
  void DrawShadow();

  ///************************* アクセッサ *************************///

  // 指定名のシーンを取得
  BaseSubScene *GetScene(const std::string &name) const;

  // 現在のシーン名を取得
  std::string GetCurrentSceneName() const {
    return currentScene_ ? currentScene_->GetSceneName() : "";
  }

  // 遷移中か確認
  bool IsTransitioning() const { return phase_ != TransitionPhase::None; }

private:
  ///************************* 内部処理 *************************///

  // 遷移演出の進行
  void UpdateTransition();

  // 現在のシーンを抜けて、指定シーンに入る
  void EnterScene(const std::string &sceneName, const SubScenePayload &payload);

  // 描画対象になる現在シーン（無ければ nullptr）
  BaseSubScene *GetDrawableScene() const;

private:
  ///************************* メンバ変数 *************************///

  // 遷移演出の段階（覆っている間に切り替え、明けたら操作再開）
  enum class TransitionPhase { None, Covering, Revealing };

  std::unordered_map<std::string, std::unique_ptr<BaseSubScene>> subScenes_;
  BaseSubScene *currentScene_ = nullptr;

  std::unique_ptr<ISceneTransition> transition_;
  TransitionPhase phase_ = TransitionPhase::None;

  // 覆い終わった時点で入るシーンと、渡すデータ
  std::string pendingSceneName_;
  SubScenePayload pendingPayload_;

  EventHandler eventHandler_;
};

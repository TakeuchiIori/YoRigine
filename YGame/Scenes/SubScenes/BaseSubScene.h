#pragma once

// C++
#include <functional>
#include <memory>
#include <string>

// Engine
#include "Drawer/LineManager/Line.h"
#include "Player/Player.h"
#include "Systems/Camera/Camera.h"

// App
#include "GameIds.h"
#include "SceneDataStructures.h"

/// <summary>
/// サブシーン基底クラス
/// GameScene の中で切り替わる場面（フィールド /
/// バトル）の共通インターフェース。
/// サブシーン同士は互いの型を知らず、受け渡しは SubScenePayload、
/// 親（GameScene）への連絡は SubSceneEvent だけで行う。
/// </summary>
class BaseSubScene {
public:
  using TransitionCallback =
      std::function<void(const SubSceneTransitionRequest &)>;
  using EventCallback = std::function<void(SubSceneEvent)>;

  BaseSubScene(const std::string &name) : sceneName_(name) {}
  virtual ~BaseSubScene() = default;

  ///************************* 基本関数 *************************///
  virtual void Initialize(YoRigine::Camera *camera, Player *player) = 0;
  virtual void Update() = 0;
  virtual void DrawObject() = 0;

  // 以下は不要なサブシーンが空実装を書かなくて済むよう、既定の実装を持つ
  virtual void DrawNonOffscreen() {}
  virtual void DrawShadow() {}
  virtual void DrawLine() {}
  virtual void DrawUI() {}
  virtual void Finalize() {}

  // 最初からやり直す（リトライ）
  virtual void Restart() {}

  ///************************* ライフサイクル *************************///

  // シーンに入る時。payload には前のシーンから渡されたデータが入っている
  // （自分宛ての型でなければ無視してよい）
  virtual void OnEnter([[maybe_unused]] const SubScenePayload &payload) {
    isActive_ = true;
  }

  // シーンから出る時
  virtual void OnExit() { isActive_ = false; }

  // 一時停止（遷移演出中など）
  virtual void OnPause() { isPaused_ = true; }

  // 再開
  virtual void OnResume() { isPaused_ = false; }

  ///************************* アクセッサ *************************///

  void SetTransitionCallback(TransitionCallback callback) {
    transitionCallback_ = std::move(callback);
  }
  void SetEventCallback(EventCallback callback) {
    eventCallback_ = std::move(callback);
  }

  bool IsActive() const { return isActive_; }
  bool IsPaused() const { return isPaused_; }
  const std::string &GetSceneName() const { return sceneName_; }

protected:
  ///************************* 派生クラスから使う *************************///

  // 別のサブシーンへの遷移を依頼する（実際の切り替えは SubSceneManager が行う）
  void RequestTransition(const std::string &targetSceneName,
                         const SubScenePayload &payload = {}) {
    if (transitionCallback_) {
      transitionCallback_({targetSceneName, payload});
    }
  }

  // よく使う遷移のヘルパー関数（in-memory コピーで受け渡し。ファイル I/O も
  // new/delete もなし）
  void RequestBattleTransition(const BattleTransitionData &data) {
    RequestTransition(SubSceneId::Battle, data);
  }
  void RequestFieldTransition(const FieldReturnData &data) {
    RequestTransition(SubSceneId::Field, data);
  }

  // 親（GameScene）へ通知する。メインシーンの遷移などはサブシーンでは決めない
  void RaiseEvent(SubSceneEvent event) {
    if (eventCallback_) {
      eventCallback_(event);
    }
  }

protected:
  ///************************* メンバ変数 *************************///

  std::string sceneName_;
  YoRigine::Camera *sceneCamera_ = nullptr;
  Player *player_ = nullptr;

  bool isActive_ = false;
  bool isPaused_ = false;

private:
  TransitionCallback transitionCallback_;
  EventCallback eventCallback_;
};

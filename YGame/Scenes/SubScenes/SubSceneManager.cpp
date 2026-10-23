#include "SubSceneManager.h"

// Engine
#include "GPUParticle/YGpuEmitManager.h"
#include "Particle/YParticleManager.h"
#include "SceneSystems/Transitions/Shatter/ShatterTransition.h"

/// <summary>
/// 初期化処理
/// </summary>
void SubSceneManager::Initialize() {
  phase_ = TransitionPhase::None;

  // サブシーン切り替えは「画面が砕ける」演出を使う
  transition_ = std::make_unique<ShatterTransition>();
  transition_->Initialize();
}

/// <summary>
/// 終了処理
/// </summary>
void SubSceneManager::Finalize() {
  pendingPayload_ = std::monostate{};

  for (auto &[name, scene] : subScenes_) {
    if (scene) {
      scene->Finalize();
    }
  }
  subScenes_.clear();
  currentScene_ = nullptr;
}

/// <summary>
/// 即時シーン切り替え（演出なし）
/// </summary>
void SubSceneManager::SwitchToScene(const std::string &sceneName,
                                    const SubScenePayload &payload) {
  if (IsTransitioning() || !GetScene(sceneName)) {
    return;
  }
  EnterScene(sceneName, payload);
}

/// <summary>
/// 演出付きシーン切り替え
/// </summary>
bool SubSceneManager::SwitchToSceneWithTransition(
    const std::string &sceneName, const SubScenePayload &payload) {
  // 遷移中の依頼は捨てる。ここで payload
  // を上書きすると、進行中の遷移が別のデータで入場してしまう
  if (IsTransitioning() || !GetScene(sceneName)) {
    return false;
  }

  pendingSceneName_ = sceneName;
  pendingPayload_ = payload;
  phase_ = TransitionPhase::Covering;
  transition_->StartCover();
  return true;
}

/// <summary>
/// 更新処理
/// </summary>
void SubSceneManager::Update() {
  // 遷移演出中はサブシーンを止め、演出だけ進める
  if (IsTransitioning()) {
    UpdateTransition();
    return;
  }

  if (currentScene_ && !currentScene_->IsPaused()) {
    currentScene_->Update();
  }
}

/// <summary>
/// 遷移演出の進行
/// </summary>
void SubSceneManager::UpdateTransition() {
  transition_->Update();
  if (!transition_->IsFinished()) {
    return;
  }

  switch (phase_) {
  case TransitionPhase::Covering:
    // 覆い終わった瞬間に中身を差し替え、明けるまでは止めておく
    EnterScene(pendingSceneName_, pendingPayload_);
    if (currentScene_) {
      currentScene_->OnPause();
    }
    pendingPayload_ = std::monostate{};
    phase_ = TransitionPhase::Revealing;
    transition_->StartReveal();
    break;

  case TransitionPhase::Revealing:
    phase_ = TransitionPhase::None;
    if (currentScene_) {
      currentScene_->OnResume();
    }
    break;

  case TransitionPhase::None:
    break;
  }
}

/// <summary>
/// 現在のシーンを抜けて、指定シーンに入る
/// </summary>
void SubSceneManager::EnterScene(const std::string &sceneName,
                                 const SubScenePayload &payload) {
  if (currentScene_) {
    currentScene_->OnExit();
    // 前のシーンで出したパーティクルを次のシーンへ持ち越さない
    YParticleManager::GetInstance().StopAndClearActiveEmitters();
    YoRigine::YGpuEmitManager::GetInstance()->StopAllEmitterGroups();
  }

  currentScene_ = GetScene(sceneName);
  if (currentScene_) {
    currentScene_->OnEnter(payload);
  }
}

/// <summary>
/// 指定シーンの取得
/// </summary>
BaseSubScene *SubSceneManager::GetScene(const std::string &name) const {
  auto it = subScenes_.find(name);
  return it != subScenes_.end() ? it->second.get() : nullptr;
}

/// <summary>
/// 描画対象になる現在シーン
/// </summary>
BaseSubScene *SubSceneManager::GetDrawableScene() const {
  return (currentScene_ && currentScene_->IsActive()) ? currentScene_ : nullptr;
}

///************************* 描画（現在のシーンへ委譲）
///*************************///

void SubSceneManager::DrawObject() {
  if (auto *scene = GetDrawableScene())
    scene->DrawObject();
}

void SubSceneManager::DrawLine() {
  if (auto *scene = GetDrawableScene())
    scene->DrawLine();
}

void SubSceneManager::DrawUI() {
  if (auto *scene = GetDrawableScene())
    scene->DrawUI();
}

void SubSceneManager::DrawNonOffscreen() {
  if (auto *scene = GetDrawableScene())
    scene->DrawNonOffscreen();
}

void SubSceneManager::DrawShadow() {
  if (auto *scene = GetDrawableScene())
    scene->DrawShadow();
}

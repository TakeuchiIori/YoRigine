#include "SceneLoadController.h"

// Engine
#include "../ObjectSelector.h"
#include "../SceneSerializer.h"
#include <Debugger/Logger.h>

namespace YoRigine {

std::string
SceneLoadController::BuildScenePath(const std::string &sceneName) const {
  return sceneFolderPath_ + sceneName + ".json";
}

void SceneLoadController::Load(const std::string &sceneName) {
  if (!context_.IsValid() || !context_.serializer) {
    return;
  }

  currentScenePath_ = BuildScenePath(sceneName);

  // 同じシーンの再ロード要求は何もしない (退避→復元で空になるのを避ける)
  if (currentSceneName_ == sceneName) {
    if (context_.selector) {
      context_.selector->ClearSelection();
    }
    Logger("[SceneLoadController] 既にアクティブなシーンです: " + sceneName);
    return;
  }

  ObjectManager *objectManager = context_.objectManager;

  // 現在のシーンを退避する。PlacedObject は pool に残したまま collider だけ
  // CollisionManager から外すので、D3D12 リソースの再確保が起きない。
  if (!currentSceneName_.empty()) {
    objectManager->StashCurrentAs(currentSceneName_);
  }

  // 退避済みがあれば JSON 再パース + CreateObject ループをまるごと省略して復元
  if (objectManager->TryRestore(sceneName)) {
    if (context_.selector) {
      context_.selector->ClearSelection();
    }
    currentSceneName_ = sceneName;
    Logger("[SceneLoadController] キャッシュから復元しました: " + sceneName);
    return;
  }

  // 初回ロード: 通常の JSON 読み込み経路
  objectManager->ClearAllObjects();
  context_.serializer->LoadScene(currentScenePath_);
  if (context_.selector) {
    context_.selector->ClearSelection();
  }
  currentSceneName_ = sceneName;
  Logger("[SceneLoadController] 読み込みました: " + sceneName);
}

bool SceneLoadController::Save() {
  if (!context_.serializer || currentScenePath_.empty()) {
    return false;
  }
  const bool ok = context_.serializer->SaveScene(currentScenePath_);
  Logger(ok ? "[SceneLoadController] 保存しました: " + currentScenePath_
            : "[SceneLoadController] 保存に失敗しました: " + currentScenePath_);
  return ok;
}

bool SceneLoadController::Reload() {
  if (!context_.serializer || currentScenePath_.empty()) {
    return false;
  }
  if (context_.selector) {
    context_.selector->ClearSelection();
  }
  return context_.serializer->LoadScene(currentScenePath_);
}

} // namespace YoRigine

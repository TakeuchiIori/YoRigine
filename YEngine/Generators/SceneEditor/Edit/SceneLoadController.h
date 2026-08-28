#pragma once

// Engine
#include "../Core/SceneEditorContext.h"

// C++
#include <string>

namespace YoRigine {

/// <summary>
/// シーンの読み込み・保存とキャッシュ切り替えを担うクラス。
///
/// 単に JSON を読むだけでなく、シーン切替時に
///   1. 現在のシーンを ObjectManager へ退避 (PlacedObject は pool に残す)
///   2. 同じシーンの退避があれば JSON 再パースを丸ごと省略して復元
/// という経路を通す。D3D12
/// リソースの再確保を避けるためで、フィールド↔バトルの
/// 往復が頻繁なこのゲームでは切り替え時間に直結する。
/// </summary>
class SceneLoadController {
public:
  explicit SceneLoadController(const SceneEditorContext &context)
      : context_(context) {}

  // シーン名 (拡張子なし) を指定して読み込む。
  void Load(const std::string &sceneName);

  // 現在開いているシーンを上書き保存する。
  bool Save();

  // 現在のシーンを JSON から読み直す (エディタの「読み込み」メニュー用)。
  bool Reload();

  const std::string &GetCurrentSceneName() const { return currentSceneName_; }
  const std::string &GetCurrentScenePath() const { return currentScenePath_; }

  // シーン JSON の置き場所。既定は "Resources/Json/Scenes/"。
  void SetSceneFolderPath(const std::string &path) { sceneFolderPath_ = path; }

private:
  std::string BuildScenePath(const std::string &sceneName) const;

  const SceneEditorContext &context_;

  std::string sceneFolderPath_ = "Resources/Json/Scenes/";
  std::string currentSceneName_;
  std::string currentScenePath_;
};

} // namespace YoRigine

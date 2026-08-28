#pragma once

#ifdef USE_IMGUI

// C++
#include <functional>
#include <string>
#include <vector>

// Engine
#include <Object3D/ObjectManager.h>

namespace YoRigine {

/// <summary>
/// モデルフォルダのスキャン・検索・一覧UI・配置ボタンを担う。
/// 配置イベントはコールバックで通知するため ObjectManager に直接依存しない。
/// </summary>
class ModelBrowser {
public:
  ModelBrowser() = default;
  ~ModelBrowser() = default;

  void SetModelFolderPath(const std::string &path);

  /// モデル配置要求時に呼ばれるコールバック（SceneEditor::PlaceObject を渡す）
  void SetPlaceCallback(std::function<void(const std::string &)> cb) {
    placeCallback_ = cb;
  }

  // ── フォルダスキャン ──────────────────────────────────────
  void ScanModelFolder();

  // ── ImGui 描画 ────────────────────────────────────────────
  void Draw();

private:
  void FilterModels();

  bool IsValidIndex(int idx) const {
    return idx >= 0 && idx < static_cast<int>(modelFiles_.size());
  }

  std::string modelFolderPath_ = "Resources/Models/";

  std::vector<std::string> modelFiles_;
  std::vector<std::string> modelNames_;
  std::vector<int> filteredIndices_;

  int selectedIndex_ = -1;
  bool showSearchBar_ = false;
  char searchBuf_[256] = {};

  std::function<void(const std::string &)> placeCallback_;
};

} // namespace YoRigine

#endif // USE_IMGUI

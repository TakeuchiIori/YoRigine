// ===========================================================
// YEditorWidget_AssetPicker.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_AssetPicker.h"
#include "YEditorWidget_ItemWidth.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace YEditorWidget {

namespace {

std::string ToLower(std::string_view text) {
  std::string lower(text);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower;
}

// パスの末尾要素だけを取り出す（一覧の表示を短くするため）
std::string_view FileNameOf(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

} // namespace

bool MatchesFilter(std::string_view text, std::string_view query) {
  if (query.empty()) {
    return true;
  }
  return ToLower(text).find(ToLower(query)) != std::string::npos;
}

//=============================================================================
// SearchBox
//=============================================================================
bool SearchBox::Draw(const char *id, const char *hint, float width) {
  std::snprintf(buffer_, sizeof(buffer_), "%s", query_.c_str());

  ImGui::SetNextItemWidth(width);
  const bool changed =
      ImGui::InputTextWithHint(id, hint, buffer_, sizeof(buffer_));
  if (changed) {
    query_ = buffer_;
  }

  if (!query_.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("×")) {
      query_.clear();
      return true;
    }
  }
  return changed;
}

bool SearchBox::Matches(std::string_view text) const {
  return MatchesFilter(text, query_);
}

//=============================================================================
// AssetCombo
//=============================================================================
bool AssetCombo(const char *label, std::string &currentPath,
                const std::vector<std::string> &candidates, bool allowEmpty,
                const char *emptyLabel) {
  bool changed = false;

  const std::string preview =
      currentPath.empty() ? emptyLabel : std::string(FileNameOf(currentPath));

  SetNextItemWidthForLabel(label);
  if (!ImGui::BeginCombo(label, preview.c_str())) {
    return false;
  }

  if (allowEmpty) {
    if (ImGui::Selectable(emptyLabel, currentPath.empty())) {
      currentPath.clear();
      changed = true;
    }
  }

  for (const auto &candidate : candidates) {
    const bool selected = (candidate == currentPath);
    if (ImGui::Selectable(candidate.c_str(), selected)) {
      currentPath = candidate;
      changed = true;
    }
    if (selected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  ImGui::EndCombo();
  return changed;
}

//=============================================================================
// AssetPickerPopup
//=============================================================================
bool AssetPickerPopup::Draw(const char *id, const char *buttonLabel,
                            std::string &currentPath,
                            const std::vector<std::string> &candidates,
                            bool allowEmpty) {
  bool changed = false;

  ImGui::PushID(id);
  if (ImGui::Button(buttonLabel)) {
    ImGui::OpenPopup("##assetPicker");
  }

  if (ImGui::BeginPopup("##assetPicker")) {
    searchBox_.Draw("##assetSearch", "検索...", 240.0f);
    ImGui::Separator();

    if (allowEmpty) {
      if (ImGui::Selectable("(なし / モデル本来の設定)", currentPath.empty())) {
        currentPath.clear();
        changed = true;
        ImGui::CloseCurrentPopup();
      }
    }

    if (ImGui::BeginChild("##assetList", ImVec2(320.0f, 260.0f))) {
      for (const auto &candidate : candidates) {
        if (!searchBox_.Matches(candidate)) {
          continue;
        }
        if (ImGui::Selectable(candidate.c_str(), candidate == currentPath)) {
          currentPath = candidate;
          changed = true;
          ImGui::CloseCurrentPopup();
        }
      }
    }
    ImGui::EndChild();
    ImGui::EndPopup();
  }
  ImGui::PopID();

  return changed;
}

//=============================================================================
// ScanAssetFiles
//=============================================================================
std::vector<std::string>
ScanAssetFiles(const std::string &folder,
               const std::vector<std::string> &extensions) {
  std::vector<std::string> results;

  std::error_code ec;
  if (!std::filesystem::exists(folder, ec)) {
    return results;
  }

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(folder, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string ext = ToLower(entry.path().extension().string());
    if (std::find(extensions.begin(), extensions.end(), ext) ==
        extensions.end()) {
      continue;
    }
    // 区切りは常に '/' に揃える。TextureManager
    // のキーがパス文字列そのものなので、 同じファイルが別キー扱いになるのを防ぐ。
    std::string path = entry.path().generic_string();
    results.push_back(std::move(path));
  }

  std::sort(results.begin(), results.end());
  return results;
}

} // namespace YEditorWidget
#endif // USE_IMGUI

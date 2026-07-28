#include "TutorialProgress.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include <json.hpp>

namespace YoRigine {

///************************* 基本関数 *************************///

TutorialProgress *TutorialProgress::GetInstance() {
  static TutorialProgress instance;
  return &instance;
}

bool TutorialProgress::Load() {
  loaded_ = true;
  seen_.clear();

  if (filePath_.empty() || !std::filesystem::exists(filePath_))
    return false;

  try {
    std::ifstream file(filePath_);
    if (!file.is_open())
      return false;

    nlohmann::json root;
    file >> root;
    for (const nlohmann::json &key :
         root.value("seen", nlohmann::json::array())) {
      seen_.insert(key.get<std::string>());
    }
    return true;
  } catch (...) {
    // 壊れた進捗ファイルで起動不能にならないよう、空の記録として続行する。
    seen_.clear();
    return false;
  }
}

bool TutorialProgress::Save() const {
  if (filePath_.empty())
    return false;

  try {
    // 差分を追いやすいよう、書き出しは名前順に並べる。
    std::vector<std::string> keys(seen_.begin(), seen_.end());
    std::sort(keys.begin(), keys.end());

    nlohmann::json root;
    root["version"] = 1;
    root["seen"] = keys;

    const std::filesystem::path path(filePath_);
    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(filePath_);
    if (!file.is_open())
      return false;
    file << root.dump(4);
    return true;
  } catch (...) {
    return false;
  }
}

///************************* 既読管理 *************************///

bool TutorialProgress::IsSeen(const std::string &tutorialName,
                              const std::string &stepName) const {
  if (stepName.empty())
    return false;
  EnsureLoaded();
  return seen_.contains(MakeKey(tutorialName, stepName));
}

void TutorialProgress::MarkSeen(const std::string &tutorialName,
                                const std::string &stepName) {
  if (stepName.empty())
    return;
  EnsureLoaded();

  const auto result = seen_.insert(MakeKey(tutorialName, stepName));
  // 既に記録済みなら書き込みを省く。
  if (result.second)
    Save();
}

void TutorialProgress::ClearAll() {
  loaded_ = true;
  seen_.clear();
  Save();
}

///************************* 設定 *************************///

void TutorialProgress::SetFilePath(const std::string &path) {
  if (path == filePath_)
    return;
  filePath_ = path;
  // 参照先が変わったので読み直させる。
  loaded_ = false;
  seen_.clear();
}

std::size_t TutorialProgress::GetSeenCount() const {
  EnsureLoaded();
  return seen_.size();
}

///************************* 内部処理 *************************///

void TutorialProgress::EnsureLoaded() const {
  if (loaded_)
    return;
  // Load は非 const だが、遅延読み込みは論理的な状態変化ではないため
  // mutable メンバを直接触る形にしている。
  const_cast<TutorialProgress *>(this)->Load();
}

std::string TutorialProgress::MakeKey(const std::string &tutorialName,
                                      const std::string &stepName) {
  return tutorialName + "/" + stepName;
}

} // namespace YoRigine

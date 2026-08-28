#include "MotionPathLibrary.h"

#include "AttackMotionFactory.h"

#include <Debugger/Logger.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>

using json = nlohmann::json;

MotionPathLibrary &MotionPathLibrary::GetInstance() {
  static MotionPathLibrary instance;
  return instance;
}

// ============================================================
// 読み込み
// ============================================================
bool MotionPathLibrary::Load(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    Logger(("[MotionPathLibrary] 経路ファイルがありません: " + path + "\n")
               .c_str());
    return false;
  }

  try {
    json j = json::parse(ifs);
    if (!j.contains("paths") || !j["paths"].is_array())
      return false;

    entries_.clear();
    for (const auto &pathJson : j["paths"]) {
      MotionPathEntry entry;
      entry.name = pathJson.value("name", std::string());
      if (entry.name.empty())
        continue;

      entry.motion = AttackMotionFactory::Create(pathJson);
      if (!entry.motion)
        continue;

      entries_.push_back(std::move(entry));
    }

    Logger(("[MotionPathLibrary] " + std::to_string(entries_.size()) +
            "件の経路を読み込みました\n")
               .c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[MotionPathLibrary] 読み込み失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}

// ============================================================
// 書き出し
// ============================================================
bool MotionPathLibrary::Save(const std::string &path) const {
  json j;
  json array = json::array();

  for (const auto &entry : entries_) {
    if (!entry.motion)
      continue;

    // 経路本体の JSON に名前を足して1エントリにする
    json pathJson = AttackMotionFactory::ToJson(*entry.motion);
    pathJson["name"] = entry.name;
    array.push_back(pathJson);
  }
  j["paths"] = array;

  try {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }
    std::ofstream ofs(path);
    if (!ofs.is_open())
      return false;
    ofs << std::setw(4) << j;
    Logger(("[MotionPathLibrary] 保存しました: " + path + "\n").c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[MotionPathLibrary] 保存失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}

// ============================================================
// 検索
// ============================================================
const IAttackMotion *MotionPathLibrary::Find(const std::string &name) const {
  for (const auto &entry : entries_) {
    if (entry.name == name)
      return entry.motion.get();
  }
  return nullptr;
}

std::shared_ptr<IAttackMotion>
MotionPathLibrary::FindShared(const std::string &name) const {
  for (const auto &entry : entries_) {
    if (entry.name == name)
      return entry.motion;
  }
  return nullptr;
}

// ============================================================
// 追加・複製・削除
// ============================================================
int MotionPathLibrary::Add(const std::string &name,
                           const std::string &typeName) {
  auto motion = AttackMotionFactory::CreateDefault(typeName);
  if (!motion)
    return -1;

  MotionPathEntry entry;
  entry.name = MakeUniqueName(name);
  entry.motion = std::move(motion);

  entries_.push_back(std::move(entry));
  return static_cast<int>(entries_.size()) - 1;
}

int MotionPathLibrary::Duplicate(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size()))
    return -1;
  if (!entries_[index].motion)
    return -1;

  MotionPathEntry entry;
  entry.name = MakeUniqueName(entries_[index].name);
  entry.motion = entries_[index].motion->Clone();

  entries_.push_back(std::move(entry));
  return static_cast<int>(entries_.size()) - 1;
}

void MotionPathLibrary::Remove(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size()))
    return;
  entries_.erase(entries_.begin() + index);
}

// ============================================================
// 一意な名前を作る
// ============================================================
std::string MotionPathLibrary::MakeUniqueName(const std::string &base) const {
  std::string candidate = base.empty() ? "path" : base;

  int suffix = 1;
  bool duplicated = true;
  while (duplicated) {
    duplicated = false;
    for (const auto &entry : entries_) {
      if (entry.name != candidate)
        continue;
      duplicated = true;
      candidate =
          (base.empty() ? "path" : base) + "_" + std::to_string(suffix++);
      break;
    }
  }
  return candidate;
}

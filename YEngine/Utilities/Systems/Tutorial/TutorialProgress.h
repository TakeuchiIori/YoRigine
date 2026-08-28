#pragma once

// C++
#include <string>
#include <unordered_set>

namespace YoRigine {

// 一度見たステップを記録し、次回以降は出さないための永続データ。
//
// キーは「チュートリアル名 / ステップ名」。ステップ名を変えると別物として
// 扱われるが、作り直したステップは見せ直したいので、これで都合が良い。
class TutorialProgress {
public:
  ///************************* 基本関数 *************************///

  static TutorialProgress *GetInstance();

  // 明示的な読み込み・保存。問い合わせ時にも未読込なら自動で読み込む。
  bool Load();
  bool Save() const;

  ///************************* 既読管理 *************************///

  bool IsSeen(const std::string &tutorialName,
              const std::string &stepName) const;

  // 既読にして即座に保存する。途中でゲームを落としても記録が残るようにする。
  void MarkSeen(const std::string &tutorialName, const std::string &stepName);

  // 記録を全消去する（デバッグ用）。ファイルへも反映する。
  void ClearAll();

  ///************************* 設定 *************************///

  void SetFilePath(const std::string &path);
  const std::string &GetFilePath() const { return filePath_; }

  std::size_t GetSeenCount() const;

private:
  ///************************* 内部処理 *************************///

  TutorialProgress() = default;
  ~TutorialProgress() = default;
  TutorialProgress(const TutorialProgress &) = delete;
  TutorialProgress &operator=(const TutorialProgress &) = delete;

  // 未読込なら読み込む。const な問い合わせからも呼べるようにしている。
  void EnsureLoaded() const;

  static std::string MakeKey(const std::string &tutorialName,
                             const std::string &stepName);

  ///************************* メンバ変数 *************************///

  mutable std::unordered_set<std::string> seen_;
  mutable bool loaded_ = false;
  std::string filePath_ = "Resources/Json/Tutorials/Progress.json";
};

} // namespace YoRigine

#pragma once

#ifdef USE_IMGUI

#include "Debugger/DopeSheet/DopeSheetEditor.h"
#include "GuardConfig.h"

#include <string>
#include <vector>

class PlayerGuard;

// ============================================================
// ガード設定エディタ
//
// 発生 / 持続 / パリィ / 硬直をフレームのタイムラインとして編集する。
// 数値だけを並べても「パリィが持続のどこにあるか」が分からず、
// 気づかないうちに持続全体がパリィになっている、といった状態を招くため、
// 攻撃エディタと同じドープシートで区間として見せる。
//
// トラックの並びはそのまま実行順:
//   発生 → 防御中 → パリィ → 硬直
// ============================================================
class GuardEditor {
public:
  // 編集対象を差し込む（PlayerGuard の設定を直接編集する）
  void SetTarget(PlayerGuard *guard) { guard_ = guard; }

  // エディタ本体の描画
  void Draw();

private:
  // GuardConfig からタイムライン表示用のトラックを作る
  void BuildTracks(const GuardTimeline &timeline);

  // ドープシートで編集された区間を GuardConfig へ書き戻す
  void ApplyTracks(GuardTimeline &timeline);

  // 通常ガード／パリィの結果パラメータを編集する
  void DrawOutcome(const char *label, GuardOutcome &outcome,
                   const char *idSuffix);

  // 現在の設定がどういうガードになるかを一行で説明する
  void DrawSummary(const GuardConfig &config) const;

private:
  PlayerGuard *guard_ = nullptr;

  DopeSheet::DopeSheetEditor dopeEditor_;
  std::vector<DopeSheet::DopeTrack> tracks_;

  // 前回描画時のタイムライン。変化を検出してトラックを組み直すために持つ。
  GuardTimeline cachedTimeline_{};
  bool hasCache_ = false;

  std::string filePath_ = GuardConfigIO::kDefaultPath;
};

#endif // USE_IMGUI

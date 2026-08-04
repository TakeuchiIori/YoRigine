#pragma once

#ifdef USE_IMGUI

#include "Debugger/DopeSheet/DopeSheetEditor.h"
#include "EnemyAttackAction.h"

#include <string>
#include <vector>

// ============================================================
// 攻撃データエディタ
//
// 攻撃は「フェーズの並び」なので、数値を縦に並べるより
// タイムライン上の区間として見せた方が構造が分かる。
// どのフェーズで攻撃判定が出るか、無敵がどこまで続くかは
// 特に時間軸で見ないと判断できない。
//
// トラック構成:
//   フェーズ   … 各フェーズを種別ごとの色の区間バーで表示
//   攻撃判定   … damageWindow が設定されたフェーズ
//   無敵       … invincible が設定されたフェーズ
// ============================================================
class EnemyAttackEditor {
public:
  void Draw();

private:
  // ── 一覧 ──
  void DrawActionList();

  // ── タイムライン ──
  void BuildTracks(const EnemyAttackAction &action);
  void ApplyTracks(EnemyAttackAction &action);
  void DrawTimeline(EnemyAttackAction &action);

  // ── フェーズ ──
  void DrawPhaseList(EnemyAttackAction &action);
  void DrawPhaseInspector(AttackPhase &phase);

  // ── 攻撃全体の設定 ──
  void DrawActionSettings(EnemyAttackAction &action);

  // 秒 ↔ フレーム変換（タイムライン表示用）
  int ToFrames(float seconds) const;
  float ToSeconds(int frames) const;

private:
  DopeSheet::DopeSheetEditor dope_;
  std::vector<DopeSheet::DopeTrack> tracks_;

  int selectedAction_ = -1;
  int selectedPhase_ = -1;

  // タイムライン表示の基準FPS。データは秒で持つのでここは表示用。
  int editorFps_ = 60;

  // トラックを組み直す必要があるか（選択変更やフェーズ増減で立てる）
  bool tracksDirty_ = true;

  char renameBuffer_[128] = {};
};

#endif // USE_IMGUI

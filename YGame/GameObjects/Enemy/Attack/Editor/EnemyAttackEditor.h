#pragma once

#ifdef USE_IMGUI

#include "../Runtime/AttackPlayer.h"
#include "../Runtime/EnemyAttackLibrary.h"
#include "AttackCurvePanel.h"
#include "AttackModifierPanel.h"

class BattleEnemyManager;
class BattleEnemy;

// ============================================================
// 攻撃エディタ
//
// 敵自身の移動・回転・スケール変化をカーブで作り、
// 相手依存の要素（判定・無敵・追尾・弾）をモディファイアで置く。
//
// プレビューは実際の戦闘中の敵で再生する。
// 停止すると再生前の姿勢へ即座に戻るので、
// 何度でも試して詰められる。
// ============================================================
class EnemyAttackEditor {
public:
  // 戦闘中の敵を触るためにマネージャを渡す
  void SetManager(BattleEnemyManager *manager) { manager_ = manager; }

  void Draw();

private:
  // ── 各部 ──
  void DrawAttackList();
  void DrawBasicSettings(EnemyAttack &attack);
  void DrawPreviewControls(EnemyAttack &attack);

  // 選択中の攻撃（無ければ nullptr）
  EnemyAttack *GetSelectedAttack();

  // プレビュー対象の敵（無ければ nullptr）
  BattleEnemy *GetPreviewEnemy() const;

  // 再生を止めて姿勢を戻す
  void StopPreview();

private:
  BattleEnemyManager *manager_ = nullptr;

  int selectedAttack_ = -1;

  AttackCurvePanel curvePanel_;
  AttackModifierPanel modifierPanel_;

  // ── プレビュー ──
  // 実際の敵を動かすので、再生器はエディタが持つ。
  AttackPlayer preview_;
  int previewEnemyIndex_ = 0;
  bool previewPlaying_ = false;
  bool previewLoop_ = true;

  // 再生中の敵。停止時に確実へ戻すため、対象が消えたかを判定する。
  BattleEnemy *previewTarget_ = nullptr;

  char nameBuffer_[128] = {};
  bool loadedOnce_ = false;
};

#endif // USE_IMGUI

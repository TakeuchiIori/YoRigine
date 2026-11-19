#pragma once

#ifdef USE_IMGUI

#include "../../Attack/Editor/AttackModifierPanel.h"
#include "../../Attack/Runtime/AttackPlayer.h"
#include "../BossAppearanceSettings.h"

#include <functional>
#include <string>
#include <vector>

class BattleEnemyManager;
class BossEnemy;
struct EnemyAttack;
namespace YoRigine {
class JsonManager;
}

// ============================================================
// ボス戦エディタ
//
// ボスの調整を1枚のパネルにまとめたもの。タブは3つ:
//
//   ボス     … 状態の確認とデバッグ操作、調整値（クリップ・移動・怯み・
//              フェーズ・カメラ）、本体の当たり判定(OBB)
//   攻撃     … ボスが使う技の選択、射程や重み、アニメクリップ、
//              攻撃判定（ジョイント・半径・区間）。AI を止めてその場で
//              再生し、判定の球を見ながら詰められる
//   出現演出 … 黒い霧の色・濃さ・タイミング。霧だけをスライダーで確認でき、
//              演出の通し再生もできる
//
// 汎用の「攻撃エディタ」は雑魚も含めた全攻撃を扱う。こちらはボス専用で、
// モデルのジョイント名・クリップ名をプルダウンで選べる点が違う。
// 打ち間違えた名前は「判定が出ない」「動かない」だけで何も教えてくれないので、
// 選ばせる方が確実に早い。
// ============================================================
class BossEditor {
public:
  // 出現演出まわりはシーンが持っているので、必要な口だけ受け取る
  struct SceneHooks {
    BossAppearanceSettings *appearance = nullptr;    // 出現演出の調整値
    YoRigine::JsonManager *appearanceJson = nullptr; // その保存先
    std::function<void(float)>
        previewFog; // 霧だけのプレビュー（0=通常→1=最暗→2=ボス戦中）
    std::function<void()> endFogPreview;    // 霧を元に戻す
    std::function<void()> replayAppearance; // 出現演出を最初から再生
    std::function<void()> spawnBossNow;     // 演出なしでボスを目の前に出す
  };

  void SetManager(BattleEnemyManager *manager) { manager_ = manager; }
  void SetSceneHooks(const SceneHooks &hooks) { hooks_ = hooks; }

  void Draw();

  // シーンを抜ける時などに、プレビューで止めた AI や姿勢を戻す
  void StopPreview();

private:
  // 生成中のボス（いなければ nullptr）
  BossEnemy *FindBoss() const;

  // ── タブ ──
  void DrawBossTab(BossEnemy *boss);
  void DrawAttackTab(BossEnemy *boss);
  void DrawAppearanceTab(BossEnemy *boss);

  // ── ボス ──
  void DrawStatus(BossEnemy &boss);
  void DrawParams(BossEnemy &boss);
  void DrawCollider(BossEnemy &boss);

  // ── 攻撃 ──
  void DrawAttackList(BossEnemy &boss);
  void DrawAttackSettings(BossEnemy &boss, EnemyAttack &attack);
  void DrawAttackPreview(BossEnemy &boss, EnemyAttack &attack);
  EnemyAttack *FindSelectedAttack();
  // ボスの技リスト（attackIds）に足す／外す。生成中の個体と敵データ表の両方を書き換える
  void SetAttackUsed(BossEnemy &boss, const std::string &id, bool used);
  void CreateBossAttack(BossEnemy &boss);

  // クリップ名・ジョイント名のプルダウン
  bool NameCombo(const char *label, std::string &value,
                 const std::vector<std::string> &names, bool allowEmpty);

private:
  BattleEnemyManager *manager_ = nullptr;
  SceneHooks hooks_;

  // ── 攻撃 ──
  // 選択は ID で持つ。攻撃の保管庫は vector なので、追加でポインタが無効になる
  std::string selectedAttackId_;
  AttackModifierPanel modifierPanel_;
  std::vector<std::string> jointNames_; // 判定の付け先の候補（モデルから取得）

  // ── 攻撃プレビュー ──
  AttackPlayer preview_;
  BossEnemy *previewTarget_ =
      nullptr; // 再生中のボス（消えたかの照合用・非所有）
  bool previewPlaying_ = false;
  bool previewLoop_ = true;
  bool aiWasPaused_ = false; // プレビュー前の AI 停止状態（終わったら戻す）

  // ── 出現演出 ──
  float fogPreviewT_ = 0.0f;
  bool fogPreviewActive_ = false;

  char newAttackName_[64] = "boss_new";
};

#endif // USE_IMGUI

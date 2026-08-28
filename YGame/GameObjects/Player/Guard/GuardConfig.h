#pragma once

#include "Vector3.h"
#include <string>

// ============================================================
// ガードのタイムライン
//
// ボタンを押してからの経過フレームで、次の順に進行する。
//
//   |<- startup ->|<-------- active -------->|<- recovery ->|
//                 |<-parry->|
//                 ^ ここから防御が成立する
//
// パリィ判定は active の内側にある区間で、ここで受けると
// 通常ガードより大きな見返りが得られる。
//
// active 全体をパリィ区間にすると「ガードすれば必ずパリィ」になり
// 読み合いが消えるので、既定では先頭の数フレームだけにしている。
// ============================================================
struct GuardTimeline {
  int fps = 60;            // このタイムラインの基準FPS（表示用）
  int startupFrames = 2;   // 押してから防御が成立するまで
  int activeFrames = 18;   // 防御が成立している長さ
  int parryStartFrame = 0; // パリィ開始（active の先頭からの相対フレーム）
  int parryEndFrame = 4;   // パリィ終了（同上）
  int recoveryFrames = 10; // 解除後の硬直

  // タイムライン全体の長さ
  int TotalFrames() const {
    return startupFrames + activeFrames + recoveryFrames;
  }

  // active 区間が始まる絶対フレーム
  int ActiveBeginFrame() const { return startupFrames; }

  // パリィ区間の絶対フレーム（タイムライン先頭基準）
  int ParryBeginFrame() const { return startupFrames + parryStartFrame; }
  int ParryEndFrameAbs() const { return startupFrames + parryEndFrame; }

  // 値の前後関係が壊れないように詰め直す（エディタ編集後に呼ぶ）
  void Sanitize();
};

// ============================================================
// 防御が成立したときの結果
//
// 通常ガードとパリィで同じ型を使い、数値だけで性格を分ける。
// これは「どちらが主導権を得たか」を数値で表現するため。
//
//   通常ガード : 受け止めたが押される  → selfPush > 0 / enemyPush = 0
//   パリィ     : 弾き返して相手が崩れる → selfPush = 0 / enemyPush > 0
//
// 演出の派手さではなく「動く側が入れ替わる」ことでプレイヤーは差を理解する。
// ============================================================
struct GuardOutcome {
  // ── ゲーム的な結果 ──
  float damageRate = 0.3f; // 通過するダメージ割合（0=完全無効 / 1=素通り）
  int ccCost = 1;          // 成立時に消費するCC
  int ccRecover = 0;       // 成立時に回復するCC

  // ── 手応え（時間停止）──
  float hitStop = 0.05f;    // ヒットストップの長さ（秒）
  float hitStopEase = 0.0f; // 停止明けに通常速へ戻す時間（0=即復帰＝硬い衝撃）

  // ── 手応え（カメラ）──
  float shakeIntensity = 0.15f;
  float shakeDuration = 0.08f;

  // ── 押し合い ──
  float selfPushDistance = 0.45f; // 自分が押し込まれる距離（m）
  float selfPushDuration = 0.12f;
  float enemyPushPower = 0.0f; // 相手を押し返す強さ
  float enemyPushDuration = 0.0f;

  // ── 盾の見た目（受け止めた瞬間のスケール変化）──
  //
  // 一度潰れてから跳ね返って戻る「パンチ」として動かす。
  // 単純に膨らませて戻すだけだと視認しづらいので、減衰する波で
  // 潰れ→行き過ぎ→静止 と動かして目に留まるようにしている。
  float shieldSquash =
      0.45f; // 変化量（0で無効）。0.4〜0.6くらいがはっきり見える
  float shieldSquashTime = 0.22f; // 元に戻るまでの秒数

  // 軸ごとの効き方。正で伸び、負で縮む。
  // 既定は「面方向(X/Y)に広がり、厚み方向(Z)に潰れる」＝受け止めた形。
  // すべて 1 にすると単純に大きくなる（漫画的なポップ）。
  Vector3 shieldSquashAxis{0.6f, 0.6f, -1.0f};

  // 波の回数。1.0 で単純な山、1.5 以上で戻りぎわに逆向きの跳ね返りが入る。
  float shieldSquashBounce = 1.6f;

  // ── エフェクト・SE ──
  std::string vfxName; // 接触点に出す Composite 名（空で無効）
};

// ============================================================
// ガード設定一式
// ============================================================
struct GuardConfig {
  GuardTimeline timeline;

  // 通常ガード成立時。受け止めるが押し込まれ、CCを消費する。
  GuardOutcome guard;

  // パリィ成立時。ダメージ無効・CC回復・相手を突き放す。
  GuardOutcome parry;

  // 正面判定。プレイヤーの正面からこの角度の外から来た攻撃は防げない。
  // 背後からの攻撃までガードできると、ガードを固めるだけで無敵になってしまう。
  float frontHalfAngleDeg = 100.0f;

  // 既定値を「通常ガード＝押される / パリィ＝押し返す」の形に整える。
  // 構造体の既定値は通常ガード寄りなので、パリィ側だけここで上書きする。
  GuardConfig();
};

// ============================================================
// JSON 入出力
//
// 攻撃データと同じく、ガード設定も専用ファイルで持つ。
// 以前は Player.json に JsonManager 経由で数フレーム分だけ混ざっていて、
// タイムライン全体を見渡せなかった。
// ============================================================
namespace GuardConfigIO {

// 既定のファイルパス
inline const char *kDefaultPath = "Resources/Json/Player/guard_config.json";

// 読み込み。ファイルが無い場合は out を既定値のままにして false を返す。
bool Load(const std::string &path, GuardConfig &out);

// 書き出し
bool Save(const std::string &path, const GuardConfig &config);

} // namespace GuardConfigIO

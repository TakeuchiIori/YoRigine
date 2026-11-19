#pragma once
#include "Vector3.h"

#include <string>

///************************* ボス出現演出の調整値 *************************///
// 最終戦の取り巻きを全滅させると、黒い霧がステージを覆い、
// カメラ正面の霧の中からボスが実体化してそのままボス戦になる。その演出の数値。
//
// 演出を再生するのは BattleScene、調整するのはボス戦エディタ。
// 両方がこの構造体だけを共有するので、エディタがシーンのクラスを知らずに済む。
struct BossAppearanceSettings {
  std::string bossEnemyId =
      "boss";                  // 出現させる敵ID。空ならボス戦なしで即クリア
  float spawnDistance = 28.0f; // プレイヤーからカメラ正面方向へ何m先に出すか

  // ── プレイヤーの立ち位置 ──
  // 演出が始まると、プレイヤーは入力を受け付けずに出現位置へ向かって歩き、
  // この距離だけ手前で立ち止まってボスの方を向く（既にこれより近ければ歩かない）。
  float playerStandDistance = 20.0f;

  // ── カメラの構図（プレイヤー越しにボスを画面中央へ捉える）──
  float cameraShoulderYaw =
      0.30f; // player→ボス軸から横へずらす角(rad)。0だと背中でボスが隠れる
  float cameraPitch =
      0.05f; // カメラの見下ろし角(rad)。小さいほど低い位置から煽る
  float cameraLookHeight =
      4.0f; // ボスが現れるまで、出現位置の何m上を見ておくか
  float cameraBlendSpeed = 3.0f; // 構図へ寄る／演出明けに戻る速さ

  // ── タイムライン（秒）──
  float fogInDuration = 2.0f;     // 黒い霧が覆い切るまで
  float spawnTime = 1.8f;         // ボスを生成する時刻（霧が濃くなった頃）
  float emergenceDuration = 2.5f; // 霧から実体化するまで
  float fogLiftStart = 3.0f;      // ボス戦用の暗さまで霧を少し晴らし始める時刻
  float fogLiftDuration = 3.0f;
  float totalDuration = 7.6f; // 演出全体（咆哮が終わる頃に操作を返す）

  // ── 霧が最も濃い時 ──
  Vector3 darkFogColor = {0.02f, 0.01f, 0.035f};
  float darkFogDensity = 0.05f;
  float darkFogStart = 3.0f;
  float darkHeightFogDensity = 3.5f;
  float darkSkyFogClamp = 0.97f;

  // ── ボス戦中（少し晴れた後）──
  Vector3 battleFogColor = {0.10f, 0.07f, 0.13f};
  float battleFogDensity = 0.012f;
  float battleFogStart = 25.0f;
  float battleHeightFogDensity = 1.6f;
  float battleSkyFogClamp = 0.8f;
};

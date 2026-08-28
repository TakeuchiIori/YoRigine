#pragma once

#include "Debugger/CurveEditor/CurveChannel.h"
#include "Vector3.h"

#include <array>
#include <string>

// ============================================================
// 攻撃中に変化させるチャンネル
//
// 攻撃の「形」は位置・回転・スケールの9本のカーブで表現する。
// 突進はZが伸びるだけ、跳躍はYが山を描くだけ、回転攻撃はRotYが回るだけで、
// どれも同じ仕組みの中の違いでしかない。
// だから「突進」「跳躍」といった種別をコードに持つ必要がない。
// ============================================================
enum class AttackChannel {
  PositionX,
  PositionY,
  PositionZ,
  RotationX,
  RotationY,
  RotationZ,
  ScaleX,
  ScaleY,
  ScaleZ,
  Count,
};

const char *AttackChannelToString(AttackChannel channel);
AttackChannel AttackChannelFromString(const std::string &name);

// そのチャンネルが「何もしない」ときの値。
// 位置と回転は 0（変化なし）、スケールは 1（等倍）。
float GetChannelDefaultValue(AttackChannel channel);

// ============================================================
// 位置をどこから取るか
//
// 直線的な動きはカーブで十分だが、S字に回り込むような経路は
// カーブ3本で作るより制御点を置いた方が早い。
// 既に作ってある経路ライブラリを位置の入力元として選べるようにする。
// ============================================================
enum class AttackPositionSource {
  Curves, // 位置カーブ(X/Y/Z)を使う
  Path,   // 経路ライブラリの名前付き経路を使う
};

const char *AttackPositionSourceToString(AttackPositionSource source);
AttackPositionSource AttackPositionSourceFromString(const std::string &name);

// ============================================================
// 攻撃のトランスフォーム変化
//
// 値はすべて「攻撃開始時の姿勢からの相対量」。
// 絶対座標で持つと立ち位置ごとに作り直しになるうえ、
// 中断やプレビュー停止で元の姿勢へ戻せなくなる。
//
//   位置   … 開始時の向きを基準にしたローカル座標（前方＝+Z）のオフセット
//   回転   … 開始時の角度からの差分（ラジアン）
//   スケール … 基準スケールに対する倍率
// ============================================================
class AttackTracks {
public:
  // 時間を 0〜1 の進行度で受け取り、各要素を評価する
  Vector3 EvaluatePositionOffset(float t) const;
  Vector3 EvaluateRotationOffset(float t) const;
  Vector3 EvaluateScaleMultiplier(float t) const;

  // 指定チャンネルにキーがあるか（無ければ評価をスキップできる）
  bool HasKeys(AttackChannel channel) const;

  CurveChannel &GetChannel(AttackChannel channel);
  const CurveChannel &GetChannel(AttackChannel channel) const;

  // 全チャンネルを空にする
  void Clear();

private:
  // キーが無いチャンネルは既定値を返す
  float EvaluateChannel(AttackChannel channel, float t) const;

private:
  std::array<CurveChannel, static_cast<size_t>(AttackChannel::Count)> channels_;
};

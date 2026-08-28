#include "AttackTracks.h"

// ============================================================
// enum <-> 文字列
// JSONを人が読める形にするための変換
// ============================================================
const char *AttackChannelToString(AttackChannel channel) {
  switch (channel) {
  case AttackChannel::PositionX:
    return "positionX";
  case AttackChannel::PositionY:
    return "positionY";
  case AttackChannel::PositionZ:
    return "positionZ";
  case AttackChannel::RotationX:
    return "rotationX";
  case AttackChannel::RotationY:
    return "rotationY";
  case AttackChannel::RotationZ:
    return "rotationZ";
  case AttackChannel::ScaleX:
    return "scaleX";
  case AttackChannel::ScaleY:
    return "scaleY";
  default:
    return "scaleZ";
  }
}

AttackChannel AttackChannelFromString(const std::string &name) {
  for (size_t i = 0; i < static_cast<size_t>(AttackChannel::Count); ++i) {
    const auto channel = static_cast<AttackChannel>(i);
    if (name == AttackChannelToString(channel))
      return channel;
  }
  return AttackChannel::PositionX;
}

// ============================================================
// 何もしないときの値
//
// 位置と回転は「変化なし＝0」、スケールは「等倍＝1」。
// ここを間違えるとキーの無いチャンネルで敵が消える（スケール0）。
// ============================================================
float GetChannelDefaultValue(AttackChannel channel) {
  switch (channel) {
  case AttackChannel::ScaleX:
  case AttackChannel::ScaleY:
  case AttackChannel::ScaleZ:
    return 1.0f;
  default:
    return 0.0f;
  }
}

const char *AttackPositionSourceToString(AttackPositionSource source) {
  return (source == AttackPositionSource::Path) ? "path" : "curves";
}

AttackPositionSource AttackPositionSourceFromString(const std::string &name) {
  return (name == "path") ? AttackPositionSource::Path
                          : AttackPositionSource::Curves;
}

// ============================================================
// チャンネル1本の評価
// ============================================================
float AttackTracks::EvaluateChannel(AttackChannel channel, float t) const {
  const CurveChannel &curve = channels_[static_cast<size_t>(channel)];
  if (curve.GetKeyCount() == 0) {
    return GetChannelDefaultValue(channel);
  }
  return curve.Evaluate(t);
}

Vector3 AttackTracks::EvaluatePositionOffset(float t) const {
  return {EvaluateChannel(AttackChannel::PositionX, t),
          EvaluateChannel(AttackChannel::PositionY, t),
          EvaluateChannel(AttackChannel::PositionZ, t)};
}

Vector3 AttackTracks::EvaluateRotationOffset(float t) const {
  return {EvaluateChannel(AttackChannel::RotationX, t),
          EvaluateChannel(AttackChannel::RotationY, t),
          EvaluateChannel(AttackChannel::RotationZ, t)};
}

Vector3 AttackTracks::EvaluateScaleMultiplier(float t) const {
  return {EvaluateChannel(AttackChannel::ScaleX, t),
          EvaluateChannel(AttackChannel::ScaleY, t),
          EvaluateChannel(AttackChannel::ScaleZ, t)};
}

bool AttackTracks::HasKeys(AttackChannel channel) const {
  return channels_[static_cast<size_t>(channel)].GetKeyCount() > 0;
}

CurveChannel &AttackTracks::GetChannel(AttackChannel channel) {
  return channels_[static_cast<size_t>(channel)];
}

const CurveChannel &AttackTracks::GetChannel(AttackChannel channel) const {
  return channels_[static_cast<size_t>(channel)];
}

void AttackTracks::Clear() {
  for (auto &curve : channels_) {
    curve.GetKeys().clear();
  }
}

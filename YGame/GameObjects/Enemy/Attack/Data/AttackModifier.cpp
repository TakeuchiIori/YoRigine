#include "AttackModifier.h"

const char *AttackModifierTypeToString(AttackModifierType type) {
  switch (type) {
  case AttackModifierType::FaceTarget:
    return "faceTarget";
  case AttackModifierType::HomingOffset:
    return "homingOffset";
  case AttackModifierType::Invincible:
    return "invincible";
  case AttackModifierType::EmitProjectile:
    return "emitProjectile";
  default:
    return "hitbox";
  }
}

AttackModifierType AttackModifierTypeFromString(const std::string &name) {
  if (name == "faceTarget")
    return AttackModifierType::FaceTarget;
  if (name == "homingOffset")
    return AttackModifierType::HomingOffset;
  if (name == "invincible")
    return AttackModifierType::Invincible;
  if (name == "emitProjectile")
    return AttackModifierType::EmitProjectile;
  return AttackModifierType::Hitbox;
}

const char *GetAttackModifierDescription(AttackModifierType type) {
  switch (type) {
  case AttackModifierType::FaceTarget:
    return "この区間だけ相手の方を向き続ける。\n溜め中に狙いを定め直す動きに使"
           "う。";
  case AttackModifierType::HomingOffset:
    return "カーブで作った軌道を相手の方向へ曲げる。\n強さを上げるほど避けにく"
           "くなる。";
  case AttackModifierType::Hitbox:
    return "接触ダメージが発生する区間。\n多段攻撃は段ごとに判定番号を変える。";
  case AttackModifierType::Invincible:
    return "この区間だけ攻撃を受け付けない。\n反撃技の踏み込みなどに使う。";
  case AttackModifierType::EmitProjectile:
    return "弾を撃つ。開始時刻に1回だけ発火する。\n弾の飛び方は弾側の定義で決ま"
           "る。";
  default:
    return "";
  }
}

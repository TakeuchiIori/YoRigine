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
  case AttackModifierType::SuperArmor:
    return "superArmor";
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
  if (name == "superArmor")
    return AttackModifierType::SuperArmor;
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
    return "攻撃判定が出る区間。\n多段攻撃は段ごとに判定番号を変える。\n"
           "ジョイントを指定すると腕や武器に追従する球になる。";
  case AttackModifierType::Invincible:
    return "この区間だけ攻撃を受け付けない。\n反撃技の踏み込みなどに使う。";
  case AttackModifierType::EmitProjectile:
    return "弾を撃つ。開始時刻に1回だけ発火する。\n弾の飛び方は弾側の定義で決ま"
           "る。";
  case AttackModifierType::SuperArmor:
    return "この区間は殴られても怯まない（HPは減る）。\n大技の振りかぶりを"
           "潰されないようにする。";
  default:
    return "";
  }
}

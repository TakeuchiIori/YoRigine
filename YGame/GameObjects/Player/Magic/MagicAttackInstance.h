#pragma once

#include "Collision/Core/BaseCollider.h"
#include "MagicActionData.h"
#include "Particle/EffectHandle.h"
#include "WorldTransform/WorldTransform.h"

#include <memory>
#include <unordered_map>

class BaseCollider;
class Player;

// ============================================================
// 実行中の魔法攻撃
// MagicActionData を 1 回再生する単位として、移動・スケール・判定寿命を持つ。
// PlayerMagicController に判定処理を置くと攻撃種別ごとの更新分岐で膨らむため、
// 「発生した攻撃自身」が当たり方を管理する。
// ============================================================
class MagicAttackInstance {
public:
  ~MagicAttackInstance();

  void Initialize(const MagicActionData &action, Player *owner);
  void Update(float deltaTime);
  void DrawCollision();

  bool IsAlive() const { return alive_; }

  void OnEnterCollision(BaseCollider *self, BaseCollider *other);
  void OnCollision(BaseCollider *self, BaseCollider *other);
  void OnExitCollision(BaseCollider *self, BaseCollider *other);
  void OnDirectionCollision(BaseCollider *self, BaseCollider *other,
                            HitDirection dir);
  void OnEnterDirectionCollision(BaseCollider *self, BaseCollider *other,
                                 HitDirection dir);

private:
  Vector3 ResolveTarget(Player &owner, const Vector3 &origin) const;
  void ApplyHit(BaseCollider *other);
  // 消滅処理。着弾VFXを一度出し、追従VFXを止め、判定を無効化する。
  void Die();

private:
  MagicActionData action_;
  Player *owner_ = nullptr;
  YoRigine::WorldTransform wt_;
  std::shared_ptr<BaseCollider> collider_;
  Vector3 origin_{};
  Vector3 target_{};
  Vector3 attackPosition_{}; // VFXと軌道の基準位置（判定オフセット前）
  float elapsedTime_ = 0.0f;
  bool alive_ = false;
  bool feedbackFired_ =
      false; // このインスタンスで手応え(ヒットストップ/シェイク)を出したか
  // 各敵コライダーへ最後にダメージを与えた elapsedTime_。
  // hitInterval と突き合わせて単発/周期ヒットを切り替える。
  std::unordered_map<BaseCollider *, float> lastHitTimes_;
  EffectHandle travelVfx_; // CPU/VfxMeshを束ねた複合VFXも追従できる統一ハンドル
  bool died_ = false;       // Die() を二重発火させないためのガード
};

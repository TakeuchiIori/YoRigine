#pragma once

#include "../Data/EnemyAttack.h"

#include "Vector3.h"

#include <vector>

class BaseEnemy;

// ============================================================
// 攻撃の再生器
//
// 攻撃データを時間で評価し、敵のトランスフォームへ適用する。
// 本番の攻撃ステートとエディタのプレビューが同じものを使うので、
// 「エディタで見た動きと実際の動きが違う」が起きない。
//
// 値はすべて開始時の姿勢からの相対量なので、
// Stop() で基準姿勢を書き戻せば一瞬で元に戻る。
// ============================================================
class AttackPlayer {
public:
  // 再生を開始する。開始時点の姿勢を基準として退避する。
  void Play(BaseEnemy &enemy, const EnemyAttack *attack);

  // 時間を進めてトランスフォームへ反映する
  void Update(BaseEnemy &enemy, float deltaTime);

  // 基準姿勢へ戻して停止する
  void Stop(BaseEnemy &enemy);

  bool IsPlaying() const { return playing_; }
  bool IsFinished() const { return finished_; }

  const EnemyAttack *GetAttack() const { return attack_; }

  float GetTime() const { return time_; }
  float GetNormalizedTime() const;

  // 任意の時刻へ飛ばす（エディタのスクラブ用）
  void Seek(BaseEnemy &enemy, float time);

  // 今このフレームで有効な攻撃判定の番号（無ければ -1）
  int GetActiveDamageWindow() const;

  // 今無敵か
  bool IsInvincibleNow() const;

private:
  // 現在時刻の姿勢を計算して敵へ書き込む
  void ApplyPose(BaseEnemy &enemy);

  // 位置オフセットを求める（カーブ or 経路）
  Vector3 EvaluatePositionOffset(const BaseEnemy &enemy,
                                 float normalizedTime) const;

  // 区間モディファイア（向き追従・追尾）を適用する
  void ApplyRangeModifiers(BaseEnemy &enemy, float deltaTime);

  // 瞬間モディファイア（弾の発射）を、またいだ時刻のぶんだけ発火する
  void FireInstantModifiers(BaseEnemy &enemy, float previousTime);

private:
  const EnemyAttack *attack_ = nullptr;

  bool playing_ = false;
  bool finished_ = false;
  float time_ = 0.0f;

  // ── 開始時の姿勢（相対量の基準）──
  Vector3 basePosition_{};
  Vector3 baseRotation_{};
  Vector3 baseScale_{1.0f, 1.0f, 1.0f};

  // 追尾で曲げた分の累積。カーブの軌道に加算する。
  Vector3 homingOffset_{};

  // 発火済みの瞬間モディファイア（添字）。同じ弾を毎フレーム撃たないため。
  std::vector<bool> instantFired_;
};

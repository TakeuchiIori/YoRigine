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

  /// <summary>
  /// 回転とスケールだけ基準へ戻して停止する。位置は攻撃結果として残す。
  ///
  /// プレビューは Stop() で完全に元へ戻すが、本番の攻撃で位置まで戻すと
  /// 突進で詰めた距離が無かったことになってしまう。
  /// </summary>
  void StopKeepPosition(BaseEnemy &enemy);

  bool IsPlaying() const { return playing_; }
  bool IsFinished() const { return finished_; }

  const EnemyAttack *GetAttack() const { return attack_; }

  float GetTime() const { return time_; }
  float GetNormalizedTime() const;

  // 任意の時刻へ飛ばす（エディタのスクラブ用）
  void Seek(BaseEnemy &enemy, float time);

  // 今このフレームで有効な「本体の」攻撃判定の番号（無ければ -1）。
  // ジョイント判定は含めない。雑魚敵の接触ダメージはこちらだけを見る。
  int GetActiveDamageWindow() const;

  // 今このフレームで有効な攻撃判定を、本体・ジョイントの区別なく out へ集める。
  // ボスはどちらも「球」として自前で重なりを調べるので、こちらを使う。
  void
  CollectActiveHitboxes(std::vector<const AttackModifier *> &out) const;

  /// <summary>
  /// true にすると Update のたびにアニメの時刻を time_ へ合わせ込む。
  /// エディタのプレビューはゲームを止めたまま再生するので、
  /// アニメ側が自力で進まない。そのためプレビューだけこれを使う。
  /// </summary>
  void SetDriveMotionClock(bool value) { driveMotionClock_ = value; }

  // 外から足し込む移動量（ボスのルートモーションなど）。
  // 位置は毎フレーム「基準位置＋カーブ」で書き直されるので、
  // 追尾と同じ積み上げに入れておかないと次のフレームで消えてしまう。
  void AddWorldOffset(const Vector3 &delta) { homingOffset_ += delta; }

  // 今無敵か
  bool IsInvincibleNow() const;

  // 今スーパーアーマー（怯まない）区間か
  bool IsSuperArmorNow() const;

  // 再生中クリップの長さ÷再生速度（秒）。クリップが無ければ 0。
  // エディタの「攻撃時間をクリップに合わせる」に使う。
  float GetClipPlayDuration() const { return clipPlayDuration_; }

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

  // 骨格アニメを再生したか。止めるときに待機モーションへ戻す必要があるかの判定
  bool playedClip_ = false;
  float clipPlayDuration_ = 0.0f;
  bool driveMotionClock_ = false;
};

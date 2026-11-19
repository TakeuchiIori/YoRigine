#pragma once
#include "../BaseEnemy.h"
#include "BattleEnemyData.h"

#include <string>

///************************* バトルに出る敵の共通基底
///*************************///
// BattleEnemyManager が「雑魚かボスか」を区別せずに扱うための窓口。
//
// BaseEnemy との違い:
//   BaseEnemy        …
//   フィールド敵も含めた「敵なら必ず持つもの」(HP・移動・被弾) BattleEnemyBase
//   … バトルに出る敵だけが持つもの(戦闘データ・描画の流儀・UI)
//
// これが無いとマネージャが BattleEnemy 型で敵を抱えることになり、
// ボスを置くにはマネージャごと複製するか、ボスに BattleEnemy を継承させて
// 雑魚専用の振る舞い(体当たり・連続被弾カウンター)まで背負わせるしかなくなる。
//
// 配置(エンカウント・フォーメーション)は全部ここ経由なので、
// ボスも雑魚と同じ JSON・同じエディタ操作で置ける。
class BattleEnemyBase : public BaseEnemy {
public:
  ~BattleEnemyBase() override = default;

  // 戦闘データをもとに初期化（scale はフィールド敵から引き継ぐ見た目スケール）
  virtual void InitializeBattleData(const BattleEnemyData &data,
                                    Vector3 position, const Vector3 &scale) = 0;

  // 頭上の体力バーなど、3D描画の後に重ねる UI
  virtual void DrawUI() {}

  // 攻撃の軌跡など、専用の PSO で描くエフェクト。
  // 敵の描画（最後にインスタンシング描画へ切り替わる）とは別の段で呼ばれる。
  virtual void DrawVfx() {}

  // 影
  virtual void DrawShadow() = 0;

  // プレイヤー敗北時の半透明フェード（インスタンシング描画の直前に呼ぶ）
  virtual void ApplyDeathFade() {}

  /// <summary>
  /// インスタンシング描画（同じモデルをまとめて1ドローコール）に乗せられるか。
  /// 骨格アニメを持つ敵は個体ごとに姿勢が違うので乗せられない。
  /// </summary>
  virtual bool UsesInstancedDraw() const { return !HasSkeletalMotion(); }

  // デバッグ表示用の状態名
  virtual const char *GetStateName() const = 0;

  const BattleEnemyData &GetEnemyData() const { return enemyData_; }
  BattleEnemyData &GetEnemyData() { return enemyData_; }

protected:
  // 戦闘データ（JSON 由来の初期値。実行時の HP は BaseEnemy が持つ）
  BattleEnemyData enemyData_;
};

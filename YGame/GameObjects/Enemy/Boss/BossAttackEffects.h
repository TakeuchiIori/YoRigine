#pragma once
#include "Vector3.h"

#include <array>
#include <memory>

namespace YoRigine {
class Camera;
class TrailMeshEmitter;
} // namespace YoRigine

///************************* ボス攻撃の演出 *************************///
// ボスの攻撃を「見た目」で伝えるための演出をまとめたクラス。
//
//   ・手の軌跡       … 攻撃判定が出ている手に、前腕→手先の幅のリボンを引く
//   ・振りの衝撃     … 判定が出た瞬間、手元に衝撃リングと火花（拳圧）
//   ・地面への衝撃   … 叩きつけ・着地の地点に、衝撃波・土煙・破片・閃光
//   ・広範囲の衝撃波 … 咆哮など半径の大きい判定を、地面を走るリングで見せる
//   ・命中           … プレイヤーに当たった位置に閃光と火花
//
// ここは見た目だけを受け持つ。いつ・どこで出すか（判定の時間帯や接地の検出）と、
// カメラの揺れ・振動のような「プレイヤー側の手応え」は BossEnemy が決める。
// 使うアセットは JSON（VfxMesh /
// YEffects）なので、色や量はエディタで詰められる。
class BossAttackEffects {
public:
  enum class Hand { Left, Right, Count };

  BossAttackEffects();
  ~BossAttackEffects();

  void Initialize(YoRigine::Camera *camera);

  ///************************* 手の軌跡 *************************///

  // 毎フレーム呼ぶ。tip=手先、root=前腕。active の間だけ軌跡を残す。
  // 軌跡は「点を足し続けて古い点から消える」作りなので、
  // 止めている間も点は渡しておく（出し始めの1本目が遠くから伸びないように）。
  void UpdateHandTrail(Hand hand, const Vector3 &tip, const Vector3 &root,
                       bool active);

  // 軌跡の更新（寿命で点を消す）と描画
  void Update(float deltaTime);
  void Draw();

  // 攻撃の中断などで、出ている軌跡をすぐ止める
  void StopTrails();

  ///************************* 一発もの *************************///

  // 攻撃判定が出た瞬間の手元の衝撃（拳圧）
  void PlaySwingBurst(const Vector3 &handPosition);

  // 叩きつけ・着地。groundPosition は地面の高さに合わせた位置
  void PlayGroundImpact(const Vector3 &groundPosition, float scale);

  // 咆哮など広範囲の衝撃波。radius は判定の半径（メートル）
  void PlayShockBlast(const Vector3 &groundPosition, float radius);

  // プレイヤーに命中した位置。guarded ならガードされた火花（色と量を控えめに）
  void PlayHitBurst(const Vector3 &hitPosition, bool guarded);

private:
  static constexpr size_t kHandCount = static_cast<size_t>(Hand::Count);

  std::array<std::unique_ptr<YoRigine::TrailMeshEmitter>, kHandCount> trails_;
  std::array<bool, kHandCount> trailPlaying_{};
};

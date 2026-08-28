#pragma once
// ===========================================================================
// YBounceModule
//
// 拡張Paramモジュール（任意ON/OFF）。指定した高さの水平面で粒子を反射させる。
// 爆散した破片が床で跳ねる表現に使う。
//
// 注意: このプロジェクトには地面コライダーが存在しないため（Ground は
// SceneEditor が配置する描画専用オブジェクト）、GPU 側から地形の高さを
// 引くことはできない。よって「反射面の高さ(groundY)をエディタで指定する
// 水平面固定」という割り切りで実装している。傾斜地で使うと破片が空中で
// 跳ねるので、平坦な床の上でのみ使うこと。
//
// GPU側は Update CS が ParticleExtParams(b2) を読み、位置更新後に
// groundY を下回った粒子を押し戻して速度Yを反転させる。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct BounceParams {
  bool isEnable = false;
  float groundY = 0.0f;     // 反射面の高さ（ワールドY）
  float restitution = 0.4f; // 反発係数。1=減衰なし、0=跳ねない
  float friction = 0.2f;    // 接地時に水平速度を削る割合（0〜1）
};

namespace YBounceModule {

void WriteTo(YGpuParticle::ParticleExtParameters &dst, const BounceParams &src);

#ifdef USE_IMGUI
bool DrawImGui(BounceParams &params);
#endif

} // namespace YBounceModule

#pragma once
// ===========================================================================
// YFlipbookModule
//
// 拡張Paramモジュール（任意ON/OFF）。1枚のスプライトシート（アトラス）を
// 行列に区切ってコマ送り再生する。爆発・炎・爆煙・衝撃波など、市販ゲームの
// エフェクトで最も多用される表現。
//
// 再生方法は2通り:
//   fps > 0  … 固定フレームレートで再生し、末尾までいったらループ
//   fps <= 0 … 粒子の寿命全体でちょうど1周する（爆発の一発再生向き）
//
// GPU側は VS が ParticleExtParams(b1) を読み、texcoord をセル1枚ぶんに
// スケールしてからセル位置へオフセットする。UVスクロールと併用した場合は
// スクロール後に frac してセル内に収める。
// ===========================================================================
#include <GPUParticle/YGpuParticle.h>

struct FlipbookParams {
	bool     isEnable = false;
	uint32_t cols = 4;    // アトラスの横のコマ数
	uint32_t rows = 4;    // アトラスの縦のコマ数
	float    fps = 0.0f;  // 0以下 = 寿命全体で1周（ループしない一発再生）
};

namespace YFlipbookModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const FlipbookParams& src);

#ifdef USE_IMGUI
	bool DrawImGui(FlipbookParams& params);
#endif

} // namespace YFlipbookModule

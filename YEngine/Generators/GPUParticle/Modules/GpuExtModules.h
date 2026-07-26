#pragma once
// ===========================================================================
// GpuExtModules
//
// 「拡張Paramモジュール」（任意ON/OFFの演出。エディタで追加・削除できるもの）を
// 1つにまとめた集約構造体。
//
// なぜまとめるか: これらは全て YGpuParticle::ParticleExtParameters という
// 単一CBVへ書き込むだけの軽量モジュールなので、YGpuEmitter へ渡す口を
// モジュールごとの引数にすると、モジュールを追加するたびに関数シグネチャが
// 伸びていく（コンポーネント方式の利点が失われる）。ここへメンバを1行足せば
// 呼び出し側は変更不要になる。
//
// JSONキーは従来どおりモジュール単位（"uvScrollParams" 等）で保存するため、
// この集約化による既存アセットの互換崩れはない。
// ===========================================================================
#include "UVScroll/YUVScrollModule.h"
#include "ScalePulse/YScalePulseModule.h"
#include "ColorFlicker/YColorFlickerModule.h"
#include "Drag/YDragModule.h"
#include "StretchByVelocity/YStretchByVelocityModule.h"
#include "Bounce/YBounceModule.h"
#include "Emissive/YEmissiveModule.h"
#include "Flipbook/YFlipbookModule.h"

struct GpuExtModules {
	UVScrollParams          uvScroll;
	ScalePulseParams        scalePulse;
	ColorFlickerParams      colorFlicker;
	DragParams              drag;
	StretchByVelocityParams stretch;
	BounceParams            bounce;
	EmissiveParams          emissive;
	FlipbookParams          flipbook;
};

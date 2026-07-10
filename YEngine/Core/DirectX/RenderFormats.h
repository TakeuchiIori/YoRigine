#pragma once
#include <dxgiformat.h>

/// <summary>
/// レンダーパイプライン共通のフォーマット定義。
///
/// シーンカラー（OffScreen）とポストエフェクトの中間バッファは HDR リニアで扱う。
/// 最終 blit（CopyImage.PS）で ACES トーンマップ → sRGB バックバッファへ出力する。
/// この定数を変えると、OffScreen / 中間RT / シーン描画PSO の RTV 形式が一括で切り替わる。
/// </summary>
namespace YoRigine {
    // シーンカラー / ポスト中間バッファの HDR フォーマット（アルファ付き 16bit float ×4）
    inline constexpr DXGI_FORMAT kSceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    // バックバッファ / UI（Sprite）/ 最終出力の LDR フォーマット
    inline constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

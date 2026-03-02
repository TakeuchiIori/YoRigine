#pragma once
#include <cstdint>

/// <summary>
/// パーティクルシステムごとのライト設定
/// </summary>
struct ParticleLightSetting {
	bool enableDirectionalLight = false;
	bool enablePointLight = false;
	bool enableSpotLight = false;

	// ライト全体への乗算係数（0.0〜1.0 で強度を絞れる）
	float directionalIntensity = 1.0f;
	float pointIntensity = 1.0f;
	float spotIntensity = 1.0f;

	// シェーディングモデル
	bool  halfLambert = false;   // trueでハーフランバート

	bool operator==(const ParticleLightSetting& o) const {
		return enableDirectionalLight == o.enableDirectionalLight
			&& enablePointLight == o.enablePointLight
			&& enableSpotLight == o.enableSpotLight
			&& directionalIntensity == o.directionalIntensity
			&& pointIntensity == o.pointIntensity
			&& spotIntensity == o.spotIntensity
			&& halfLambert == o.halfLambert;
	}
	bool operator!=(const ParticleLightSetting& o) const { return !(*this == o); }
};

/// <summary>
/// ビルボードのタイプ
/// </summary>
enum class BillboardType : uint32_t
{
	None = 0,        // ビルボードなし（通常のメッシュ描画）
	Full = 1,        // フルビルボード（完全にカメラを向く）
	YAxisFixed = 2   // Y軸固定ビルボード（地面エフェクト用）
};

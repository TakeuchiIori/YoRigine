#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>

// Math
#include "Vector3.h"

/// <summary>
/// マテリアル：ディゾルブ用 cbuffer ラッパ。
/// HLSL 側の MaterialDissolve cbuffer と完全同一レイアウト（16 byte 境界注意）。
/// </summary>
class MaterialDissolve
{
public:
	///************************* GPU用の構造体 *************************///

	struct DissolveData {
		float   threshold = 0.0f;    // 0=表示、1=完全消去
		float   edgeWidth = 0.08f;   // エッジ帯幅
		int     enabled = 0;         // 0=無効
		float   noiseScale = 6.0f;   // ノイズの空間スケール
		Vector3 edgeColor = { 1.8f, 0.7f, 0.15f }; // オレンジ系発光
		float   _pad0 = 0.0f;
	};

	///************************* 基本的な関数 *************************///

	void Initialize();
	void RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT index);

public:
	///************************* アクセッサ *************************///

	void  SetEnabled(bool enable) { data_->enabled = enable ? 1 : 0; }
	bool  IsEnabled() const { return data_->enabled != 0; }

	void  SetThreshold(float t) { data_->threshold = t; }
	float GetThreshold() const { return data_->threshold; }

	void  SetEdgeWidth(float w) { data_->edgeWidth = w; }
	float GetEdgeWidth() const { return data_->edgeWidth; }

	void  SetEdgeColor(const Vector3& c) { data_->edgeColor = c; }
	const Vector3& GetEdgeColor() const { return data_->edgeColor; }

	void  SetNoiseScale(float s) { data_->noiseScale = s; }
	float GetNoiseScale() const { return data_->noiseScale; }

private:
	///************************* メンバ変数 *************************///

	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	DissolveData* data_ = nullptr;
};

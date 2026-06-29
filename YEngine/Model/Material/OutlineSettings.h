#pragma once

// C++
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "Vector3.h"

namespace YoRigine {
	class JsonManager;
}

/// <summary>
/// インバートハル（背面押し出し）輪郭線のグローバル設定。
/// 全オブジェクトの Object3d::Draw が参照するため、1か所（エディタの
/// 「輪郭線」パネル）で太さ・色・有効を統一して調整できる。
/// ToonSettings と異なり、シェーダーが直接読む定数バッファ(GPU)を内部に持つ。
/// </summary>
class OutlineSettings
{
public:
	// HLSL の Outline cbuffer と一致させる（16バイト境界）。
	struct OutlineCB {
		float color[4];          // 線の色(RGBA)
		float width;             // 押し出し量(スクリーン基準)
		int   enable;            // 0=無効
		int   useDistanceFade;   // 距離フェード有効
		float distanceFadeStart; // フェード開始(ビュー空間距離)
		float distanceFadeEnd;   // フェード終了。これより遠いと太さ0
		float _pad[2];
	};

	static OutlineSettings* GetInstance();

	// 定数バッファ生成 + JSON 永続化のセットアップ（起動時に1回、DirectX 初期化後）
	void Initialize();

	// 現在のパラメータを定数バッファへ書き込む（描画前に1回）
	void UpdateCB();

	// 描画でバインドするための定数バッファリソース
	ID3D12Resource* GetResource() const { return resource_.Get(); }

	bool IsEnabled() const { return enable_ != 0; }

	// エディタUI（Editor::RegisterGameUI で登録）
	void ImGui();

private:
	OutlineSettings() = default;
	~OutlineSettings() = default;
	OutlineSettings(const OutlineSettings&) = delete;
	OutlineSettings& operator=(const OutlineSettings&) = delete;

	// パラメータ（CPU 側の真値）
	int     enable_ = 0;
	float   width_ = 0.005f;                 // スクリーン基準のおおよその太さ
	Vector3 color_ = { 0.05f, 0.05f, 0.06f }; // ほぼ黒のインク色
	// 距離フェード（遠ざかると太さを徐々に 0 へ。真っ黒化防止）
	int     useDistanceFade_ = 0;
	float   fadeStart_ = 20.0f;              // ここから細くなり始める(ビュー空間距離)
	float   fadeEnd_ = 60.0f;                // これより遠いと太さ0

	// GPU 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	OutlineCB* mapped_ = nullptr;

	std::unique_ptr<YoRigine::JsonManager> json_;
};

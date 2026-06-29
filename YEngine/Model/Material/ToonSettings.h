#pragma once

// C++
#include <memory>

#include "Vector3.h"

namespace YoRigine {
	class JsonManager;
}

/// <summary>
/// トゥーン（セルシェーディング）のグローバル設定。
/// 全オブジェクトの MaterialLighting がこの値を参照するため、
/// 1か所（エディタの「トゥーン」パネル）で全体を統一して調整できる。
/// </summary>
class ToonSettings
{
public:
	// HLSL / MaterialLight CB に流し込むパラメータ
	struct Params {
		int   enableToon = 0;            // セルシェーディング有効
		int   toonBands = 2;             // 階調数 (2〜)。2 でアニメ塗りの基本2トーン
		int   enableRim = 0;             // リムライト有効
		int   enableToonSpecular = 0;    // トゥーンスペキュラ有効
		float rimPower = 4.0f;           // リムの鋭さ
		float rimIntensity = 1.0f;       // リムの強さ
		float toonSpecularThreshold = 0.5f; // スペキュラしきい値
		float toonEdgeSoftness = 0.0f;   // バンド境界の smoothstep 幅。0 = Blender「一定」相当の完全ステップ
		// --- 2D表現（アニメ塗り） ---
		float   shadowStrength = 1.0f;            // 影色の強さ
		Vector3 shadowColor = { 0.55f, 0.55f, 0.70f }; // 影の色
		Vector3 highlightColor = { 1.0f, 1.0f, 1.0f }; // 固定ハイライトの色
	};

	static ToonSettings* GetInstance();

	// JsonManager による永続化のセットアップ（起動時に1回）
	void Initialize();

	// 現在のパラメータ
	const Params& Get() const { return params_; }

	// エディタUI（Editor::RegisterGameUI で登録）
	void ImGui();

private:
	ToonSettings() = default;
	~ToonSettings() = default;
	ToonSettings(const ToonSettings&) = delete;
	ToonSettings& operator=(const ToonSettings&) = delete;

	Params params_;
	std::unique_ptr<YoRigine::JsonManager> json_;
};

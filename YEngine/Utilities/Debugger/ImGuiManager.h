#pragma once

// C++
#ifdef USE_IMGUI
#include "imgui.h"
#include <wrl.h>
#include <d3d12.h>
#endif

// Forward Declaration
class WinApp;

namespace YoRigine {
	class DirectXCommon;
}

// =============================================================================
//  ImGuiManager
//  ImGui の初期化・描画・終了処理を一括管理するシングルトン
//  テーマ : "Black Gold" ― 漆黒 × 金箔アクセント
// =============================================================================
class ImGuiManager
{
public:
	///======================== シングルトン ========================///
	static ImGuiManager* GetInstance();
	ImGuiManager() = default;
	~ImGuiManager() = default;

	///======================== 基本関数 ============================///
	void Initialize(WinApp* winApp, YoRigine::DirectXCommon* dxCommon);
	void Begin();
	void End();
	void Draw();
	void Finalize();

#ifdef USE_IMGUI
	// --------- フォントアクセッサ -----------------------------------------
	// 通常フォント (14px, JP+Icon マージ済み)
	ImFont* GetFontNormal() const { return fontNormal_; }
	// 大フォント (18px, ツールバー / 見出し用)
	ImFont* GetFontLarge()  const { return fontLarge_; }
	// 小フォント (11px, デバッグ情報 / ステータスバー用)
	ImFont* GetFontSmall()  const { return fontSmall_; }

	// --------- テーマカラー取得 (外部 UI でも色統一したいとき) --------------
	static ImVec4 GetAccentColor() { return ImVec4(0.16f, 0.48f, 0.88f, 1.00f); } // Tech Blue
	static ImVec4 GetAccentColorDim() { return ImVec4(0.16f, 0.48f, 0.88f, 0.30f); } // Blue Alpha
	static ImVec4 GetBgDeep() { return ImVec4(0.02f, 0.02f, 0.02f, 1.00f); } // ほぼ黒
#endif

private:
	///======================== 内部処理 ============================///
	void InitialzeDX12();
	void CustomizeColor();
	void CustomizeEditor();

	///======================== シングルトン制御 =====================///
	static ImGuiManager* instance;
	ImGuiManager(ImGuiManager&) = delete;
	ImGuiManager& operator=(ImGuiManager&) = delete;

#ifdef USE_IMGUI
	///======================== メンバ変数 ==========================///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	WinApp* winApp_ = nullptr;

	ImFont* fontNormal_ = nullptr; //  14px 通常
	ImFont* fontLarge_ = nullptr; //  18px 大見出し
	ImFont* fontSmall_ = nullptr; //  11px 小テキスト
#endif
};
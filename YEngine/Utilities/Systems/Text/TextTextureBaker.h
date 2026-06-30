#pragma once

// C++
#include <string>

// Engine（数学型）
#include "Vector2.h"
#include "Vector4.h"

namespace YoRigine {

	// ============================================================
	// テキスト→PNG ベイク パラメータ
	//   文字列をフォント・サイズ・アウトライン付きで画像に焼き、UI 用テクスチャ
	//   として保存するための設定。色は RGBA 0..1。サイズ系は px(DIP, 96dpi 基準)。
	// ============================================================
	struct TextBakeParams {
		std::string text;                       // 焼く文字列（UTF-8）

		// フォント指定：fontFilePath があればそれを優先（.ttf/.otf）。無ければ fontFamily（システム名）。
		std::string fontFamily   = "Meiryo";    // システムフォントのファミリ名
		std::string fontFilePath = "";          // 任意の .ttf/.otf パス（指定時はこちら優先）

		float fontSize = 64.0f;                 // フォントサイズ(px)
		bool  bold     = false;
		bool  italic   = false;

		Vector4 fillColor = { 1.0f, 1.0f, 1.0f, 1.0f };  // 塗り色 RGBA

		// アウトライン（0 で無し）。太さはグリフ輪郭の外側に出る見た目上の太さ(px)。
		float   outlineWidth = 0.0f;
		Vector4 outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		// ドロップシャドウ（任意）
		bool    shadow       = false;
		Vector2 shadowOffset = { 3.0f, 3.0f };
		Vector4 shadowColor  = { 0.0f, 0.0f, 0.0f, 0.5f };

		float padding  = 8.0f;   // 文字の四方に空ける余白(px)。アウトライン/影のはみ出し受けも兼ねる
		float maxWidth = 0.0f;   // 0=単一行で自動幅 / >0=この幅で折り返し
		int   align    = 0;      // 折り返し時の揃え 0=左 1=中央 2=右
	};

	// ============================================================
	// テキストテクスチャ ベイカー
	//   DirectWrite + Direct2D + WIC でテキストを RGBA ビットマップに描画し、
	//   PNG として保存する。D3D デバイス不要（WIC ビットマップへ直接描画）。
	//   生成した PNG は TextureManager / Sprite / UIBase からそのまま使える。
	// ============================================================
	class TextTextureBaker {
	public:
		// テキストを PNG にベイクして outPngPath に書き出す。成功で true。
		// 画像サイズはテキスト境界＋余白から自動算出する。
		static bool Bake(const TextBakeParams& params, const std::string& outPngPath);

		// 角丸四角の「枠」テクスチャ（中央は透明・縁だけ）を生成して PNG 保存する。
		// ボタンの枠飾りなどに使う。cornerRadius=0 で直角の四角枠。
		static bool BakeRoundedRectFrame(int width, int height, float cornerRadius,
			float borderThickness, const Vector4& color, const std::string& outPngPath);

		// ベイクして、その PNG を UIBase として UIManager に登録する便利関数。
		// 既に同 id があれば貼り替える。成功で true。
		static bool BakeAndRegisterUI(const std::string& uiId,
			const TextBakeParams& params,
			const std::string& outPngPath,
			int layer = 0);

#ifdef USE_IMGUI
		// デバッグ用ベイクパネル（MyGame から Editor::RegisterGameUI で登録して使う）
		static void ImGuiPanel();
#endif
	};

} // namespace YoRigine

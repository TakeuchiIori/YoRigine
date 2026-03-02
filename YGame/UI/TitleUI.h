#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>

/// <summary>
/// タイトルシーンで表示するUIクラス
/// </summary>
class TitleUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void Draw();

private:
	///************************* 内部処理 *************************///
	UIBase* startUiController_ = nullptr;  // コントローラー接続時のAボタンUI
	UIBase* startUiKeyboard_ = nullptr;    // 未接続時のキーボードUI
	bool lastControllerConnected_ = false; // 前フレームの接続状態を保存

	// フェード関連
	float alpha_ = 1.0f;
	float fadeSpeed_ = 2.0f;
	bool isFadingOut_ = true;
	float minAlpha_ = 0.0f;
	float maxAlpha_ = 1.0f;
};

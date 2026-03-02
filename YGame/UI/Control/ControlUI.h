#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>

#include <list>
/// <summary>
/// ゲームシーンで表示するUIクラス
/// </summary>
class ControlUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void Draw();
private:
	///************************* 内部関数 *************************///
	
	// 波紋エフェクトの更新と削除
	void UpdateRipples();
	// 波紋を特定のボタン位置で再生する
	void TriggerRipple(UIBase* targetButton);
private:
	///************************* メンバ変数 *************************///

	UIBase* button_[3];
	UIBase* ripples = nullptr;
	std::list<std::unique_ptr<UIBase>> activeRipples_;
	// 波紋のテクスチャパス（キャッシュ用）
	std::string rippleTexturePath_;

	float duration_ = 0.3f;
	Vector2 originalSize_{};
	Vector2 pushSize_ = { 80.0f ,80.0f };

	bool isVisble_ = true;
};

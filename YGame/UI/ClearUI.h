#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>

/// <summary>
/// クリアシーンで表示するUIクラス
/// </summary>
class ClearUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void Draw();
	void DrawAll();

private:
	///************************* 内部処理 *************************///
	UIBase* clear_ = nullptr;
};


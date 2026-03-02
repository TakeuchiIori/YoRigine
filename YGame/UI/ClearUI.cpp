#include "ClearUI.h"
#include "Systems/GameTime/GameTime.h"
#include <Systems/Input/Input.h>

/// <summary>
/// 初期化処理
/// </summary>
void ClearUI::Initialize()
{
	YoRigine::UIManager::GetInstance()->LoadScene("ClearScene");

	// Aボタン表示（コントローラー接続時）
	clear_ = YoRigine::UIManager::GetInstance()->GetUI("Clear");

}

/// <summary>
/// 更新処理
/// </summary>
void ClearUI::Update()
{
	YoRigine::UIManager::GetInstance()->UpdateAll();
}

/// <summary>
/// 描画処理
/// </summary>
void ClearUI::Draw()
{

}

/// <summary>
/// 全UI描画処理
/// </summary>
void ClearUI::DrawAll()
{
	YoRigine::UIManager::GetInstance()->DrawAll();
}
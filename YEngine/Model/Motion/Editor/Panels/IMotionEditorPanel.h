#pragma once
#include "../MotionEditorContext.h"
class IMotionEditorPanel
{
public:
	virtual ~IMotionEditorPanel() = default;

	/// <summary>
	/// パネルの初期化
	/// </summary>
	/// <param name="context">共有コンテキストへのポインタ</param>
	virtual void Initialize(MotionEditorContext* context) = 0;

	/// <summary>
	/// 毎フレームのロジック更新
	/// </summary>
	virtual void Update() {}

	/// <summary>
	/// ImGuiによる描画処理
	/// </summary>
	virtual void DrawImGui() = 0;

	/// <summary>
	/// パネルが有効かどうか
	/// </summary>
	virtual bool IsActive() const { return isActive_; }

	/// <summary>
	/// パネルの有効/無効を切り替える
	/// </summary>
	virtual void SetActive(bool active) { isActive_ = active; }

protected:
	bool isActive_ = true;
};
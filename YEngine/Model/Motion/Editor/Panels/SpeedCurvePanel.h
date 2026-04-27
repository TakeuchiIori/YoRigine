#pragma once
#include "IMotionEditorPanel.h"

#ifdef USE_IMGUI
#include <ImCurveEdit.h>
#include <vector>
#include <array>

// ============================================================
// ImCurveEdit デリゲート
// Motion::SpeedCurve を ImCurveEdit に橋渡しする
// ============================================================
struct SpeedCurveDelegate : ImCurveEdit::Delegate
{
	// 編集中の制御点 (x = 正規化時間 [0,1], y = 速度倍率)
	std::vector<ImVec2> points;

	// --- ImCurveEdit::Delegate インターフェース ---
	size_t    GetCurveCount()                    override { return 1; }
	bool      IsVisible(size_t)                  override { return true; }
	ImVec2    GetMax()                           override { return { 1.0f, maxSpeed_ }; }
	ImVec2    GetMin()                           override { return { 0.0f, 0.0f }; }
	size_t    GetPointCount(size_t)              override { return points.size(); }
	uint32_t  GetCurveColor(size_t)              override { return 0xFF44DDFF; }
	ImVec2*   GetPoints(size_t)                  override { return points.data(); }

	int EditPoint(size_t /*curveIdx*/, int pointIdx, ImVec2 value) override
	{
		points[pointIdx] = ClampPoint(value);
		SortValues(0);
		// ソート後の同一点インデックスを返す
		for (int i = 0; i < static_cast<int>(points.size()); ++i) {
			if (points[i].x == value.x) return i;
		}
		return pointIdx;
	}

	void AddPoint(size_t /*curveIdx*/, ImVec2 value) override
	{
		points.push_back(ClampPoint(value));
		SortValues(0);
	}

	// Y 軸上限を外部から設定できるようにする
	void SetMaxSpeed(float v) { maxSpeed_ = v; }
	float GetMaxSpeed() const { return maxSpeed_; }

	void SortValues(size_t) override
	{
		std::sort(points.begin(), points.end(),
			[](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });
	}

private:
	ImVec2 ClampPoint(ImVec2 v) const
	{
		v.x = std::clamp(v.x, 0.0f, 1.0f);
		v.y = std::clamp(v.y, 0.0f, maxSpeed_);
		return v;
	}

	float maxSpeed_ = 4.0f;
};

#endif // USE_IMGUI

// ============================================================
// SpeedCurvePanel
// タイムスケールカーブの編集・ランタイム適用・焼き込みを担当
// ============================================================
class SpeedCurvePanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void DrawImGui() override;

private:
	// カーブ <-> Motion::SpeedCurve の同期
	void PullFromMotion();   // Motion → delegate_
	void PushToMotion();     // delegate_ → Motion

	// 焼き込み処理
	void BakeSpeedCurve();

	MotionEditorContext* context_ = nullptr;

#ifdef USE_IMGUI
	SpeedCurveDelegate delegate_;
	ImCurveEdit::EditInfo editInfo_{};
	float maxSpeedEdit_ = 4.0f;    // Y軸上限のUI編集用
#endif

	bool isDirty_ = false;         // Pushが必要な状態か
	Motion* lastMotion_ = nullptr; // モーション切り替え検知用
};

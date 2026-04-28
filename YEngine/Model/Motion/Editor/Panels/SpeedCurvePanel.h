#pragma once
#include "IMotionEditorPanel.h"

#ifdef USE_IMGUI
#include <ImCurveEdit.h>
#include <imgui.h>
#include <vector>
#include <algorithm>

// ============================================================
// SpeedCurveDelegate
// Motion::SpeedCurve を ImCurveEdit::Delegate に橋渡しする
// ============================================================
struct SpeedCurveDelegate : ImCurveEdit::Delegate
{
    // 編集中の制御点 (x = 正規化時間 [0,1], y = 速度倍率)
    std::vector<ImVec2> points;

    // GetMin/GetMax は参照を返す必要があるためメンバ変数で保持
    ImVec2 boundsMin{ 0.0f, 0.0f };
    ImVec2 boundsMax{ 1.0f, 4.0f };

    // --- ImCurveEdit::Delegate 実装 ---
    size_t   GetCurveCount()                           override { return 1; }
    bool     IsVisible(size_t)                         override { return true; }
    ImCurveEdit::CurveType GetCurveType(size_t) const override { return ImCurveEdit::CurveLinear; }
    ImVec2& GetMin()                                  override { return boundsMin; }
    ImVec2& GetMax()                                  override { return boundsMax; }
    size_t   GetPointCount(size_t)                     override { return points.size(); }
    uint32_t GetCurveColor(size_t)                     override { return 0xFF44DDFF; }
    ImVec2* GetPoints(size_t)                         override { return points.data(); }
    unsigned int GetBackgroundColor()                  override { return 0xFF202020; }

    int EditPoint(size_t, int pointIdx, ImVec2 value) override
    {
        value = Clamp(value);
        points[pointIdx] = value;
        // ソート後に同じ点のインデックスを探して返す
        std::sort(points.begin(), points.end(),
            [](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });
        for (int i = 0; i < static_cast<int>(points.size()); ++i) {
            if (points[i].x == value.x && points[i].y == value.y) return i;
        }
        return pointIdx;
    }

    void AddPoint(size_t, ImVec2 value) override
    {
        points.push_back(Clamp(value));
        std::sort(points.begin(), points.end(),
            [](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });
    }

    void SetMaxSpeed(float v)
    {
        maxSpeed_ = std::max(v, 1.0f);
        boundsMax.y = maxSpeed_;
    }
    float GetMaxSpeed() const { return maxSpeed_; }

    void Sort()
    {
        std::sort(points.begin(), points.end(),
            [](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });
    }

private:
    ImVec2 Clamp(ImVec2 v) const
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
// ============================================================
class SpeedCurvePanel : public IMotionEditorPanel
{
public:
    void Initialize(MotionEditorContext* context) override;
    void DrawImGui() override;

private:
    void PullFromMotion();
    void PushToMotion();
    void BakeSpeedCurve();

    MotionEditorContext* context_ = nullptr;

#ifdef USE_IMGUI
    SpeedCurveDelegate              delegate_;
    ImVector<ImCurveEdit::EditPoint> selection_;  // Edit() が要求する型
    float maxSpeedEdit_ = 4.0f;
#endif

    bool    isDirty_ = false;
    Motion* lastMotion_ = nullptr;
};
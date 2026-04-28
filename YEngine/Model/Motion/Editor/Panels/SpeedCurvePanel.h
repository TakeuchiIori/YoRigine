#pragma once
#include "IMotionEditorPanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <vector>
#include <algorithm>
#include <cmath>

// ============================================================
// SpeedCurveDelegate
// ImCurveEdit は使わず、独自カーブエディタで点を管理する
// ============================================================
struct SpeedCurveDelegate
{
    std::vector<ImVec2> points;

    float boundsMinX = 0.0f;
    float boundsMaxX = 1.0f;
    float boundsMinY = 0.0f;
    float boundsMaxY = 4.0f;

    void SetMaxSpeed(float v)
    {
        maxSpeed_ = std::max(v, 1.0f);
        boundsMaxY = maxSpeed_;
    }
    float GetMaxSpeed() const { return maxSpeed_; }

    // 点を追加（X重複チェック付き）
    bool AddPoint(ImVec2 value, float dupThresh = 0.03f)
    {
        value = Clamp(value);
        for (const auto& p : points)
            if (std::abs(p.x - value.x) < dupThresh) return false;
        points.push_back(value);
        Sort();
        return true;
    }

    // 点を削除（インデックス指定）
    void RemovePoint(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(points.size())) return;
        // 端点（index 0, last）は削除させない
        if (idx == 0 || idx == static_cast<int>(points.size()) - 1) return;
        points.erase(points.begin() + idx);
    }

    // 端点を強制配置（X=0, X=1 は常に存在）
    void EnsureEndpoints()
    {
        if (points.empty()) { points = { { 0.f, 1.f }, { 1.f, 1.f } }; return; }
        // X=0 がなければ先頭を補正
        points.front().x = 0.0f;
        points.back().x = 1.0f;
    }

    void Sort()
    {
        std::sort(points.begin(), points.end(),
            [](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });
    }

    // 指定スクリーン座標に最も近い点のインデックスを返す
    int HitTest(ImVec2 screenPos, ImVec2 rMin, ImVec2 rMax, float radius = 8.0f) const
    {
        int   best = -1;
        float bestD = radius * radius;
        for (int i = 0; i < static_cast<int>(points.size()); ++i) {
            ImVec2 sp = ToScreen(points[i], rMin, rMax);
            float dx = sp.x - screenPos.x;
            float dy = sp.y - screenPos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD) { bestD = d2; best = i; }
        }
        return best;
    }

    // スクリーン座標 → カーブ空間
    ImVec2 ToLocal(ImVec2 sp, ImVec2 rMin, ImVec2 rMax) const
    {
        float w = rMax.x - rMin.x;
        float h = rMax.y - rMin.y;
        return {
            (sp.x - rMin.x) / w,
            boundsMaxY - (sp.y - rMin.y) / h * (boundsMaxY - boundsMinY)
        };
    }

    // カーブ空間 → スクリーン座標
    ImVec2 ToScreen(ImVec2 p, ImVec2 rMin, ImVec2 rMax) const
    {
        float w = rMax.x - rMin.x;
        float h = rMax.y - rMin.y;
        float yRange = boundsMaxY - boundsMinY;
        return {
            rMin.x + std::clamp(p.x, 0.f, 1.f) * w,
            rMax.y - (std::clamp(p.y, boundsMinY, boundsMaxY) - boundsMinY) / yRange * h
        };
    }

    ImVec2 Clamp(ImVec2 v) const
    {
        v.x = std::clamp(v.x, 0.0f, 1.0f);
        v.y = std::clamp(v.y, boundsMinY, boundsMaxY);
        return v;
    }

    // 線形補間でY値を取得（EvaluateSpeedCurve の簡易版、プレビュー用）
    float Evaluate(float x) const
    {
        if (points.size() < 2) return 1.0f;
        if (x <= points.front().x) return points.front().y;
        if (x >= points.back().x)  return points.back().y;
        for (int i = 0; i + 1 < static_cast<int>(points.size()); ++i) {
            if (x <= points[i + 1].x) {
                float t = (x - points[i].x) / (points[i + 1].x - points[i].x);
                return points[i].y + t * (points[i + 1].y - points[i].y);
            }
        }
        return 1.0f;
    }

private:
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

#ifdef USE_IMGUI


    // カスタムカーブエディタ描画（戻り値: dirty になったか）
    bool DrawCurveEditor(ImVec2 size);

    // 焼き込み前スナップショットオーバーレイ
    void DrawBakedPreviewOverlay(ImVec2 rectMin, ImVec2 rectMax) const;
#endif // USE_IMGUI

    MotionEditorContext* context_ = nullptr;

#ifdef USE_IMGUI
    SpeedCurveDelegate  delegate_;
    float               maxSpeedEdit_ = 4.0f;

    // ドラッグ状態
    int   dragIndex_ = -1;   // ドラッグ中の点インデックス
    bool  dragging_ = false;

    // 右クリックメニュー用
    int   rightClickIdx_ = -1;   // 右クリックした点インデックス（-1=空白）
    ImVec2 rightClickLocalPos_{};// 右クリック位置のカーブ空間座標

    // 焼き込みプレビュー
    std::vector<ImVec2> bakedSnapshot_;
    bool                hasBakedSnapshot_ = false;
    bool                showBakedPreview_ = true;
#endif

    bool    isDirty_ = false;
    bool    bakeConfirmPending_ = false;
    Motion* lastMotion_ = nullptr;
};
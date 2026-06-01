#pragma once

#ifdef USE_IMGUI

#include <Debugger/Gizmo/IGizmable.h>
#include "FieldEnemyManager.h"
#include <functional>
#include <string>

// FieldEnemySpawnData* を IGizmable にラップしてビューポート上で
// 3D ハンドル (ImGuizmo) で動かせるようにするアダプター。
// スポーンポイントは position のみ意味を持つので Rotation/Scale は no-op。
class SpawnPointGizmable : public IGizmable
{
public:
    SpawnPointGizmable(FieldEnemySpawnData* data, std::function<void()> onEnd)
        : data_(data), onEnd_(std::move(onEnd)) {}

    std::string GetGizmoLabel() const override {
        return data_ ? ("spawn##" + data_->id) : "(null)";
    }

    Vector3 GetGizmoPosition() const override {
        return data_ ? data_->position : Vector3{};
    }
    void SetGizmoPosition(const Vector3& p) override {
        if (data_) data_->position = p;
    }

    // 回転・スケールは持たない (translate 専用)
    Vector3 GetGizmoRotation() const override { return {}; }
    void SetGizmoRotation(const Vector3&) override {}
    Vector3 GetGizmoScale() const override { return { 1.0f, 1.0f, 1.0f }; }
    void SetGizmoScale(const Vector3&) override {}

    void OnGizmoManipulationEnd() override { if (onEnd_) onEnd_(); }

    float GetGizmoPickRadius() const override { return 0.7f; }

private:
    FieldEnemySpawnData* data_ = nullptr;
    std::function<void()> onEnd_;
};

#endif // USE_IMGUI

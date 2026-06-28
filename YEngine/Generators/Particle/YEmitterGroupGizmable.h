#pragma once
#ifdef USE_IMGUI

#include "Debugger/Gizmo/IGizmable.h"
#include "YEmitterGroup.h"
#include <cmath>

/// <summary>
/// YEmitterGroup を GizmoController で操作するためのアダプタ。
///
/// ■ Position
///   group_->SetPosition(pos) を呼ぶ。
///   YEmitterGroup::SetPosition はオフセット維持で全エミッターを移動するため
///   グループ全体を1ギズモで動かすだけでよい。
///
/// ■ Rotation
///   YEmitterGroup は回転を持たないためアダプタ内でローカル保持。
///   Y軸回転のみ全エミッターのオフセット位置に差分適用する。
///   （フルオイラーが必要なら ApplyRotationDelta を拡張）
///
/// ■ Scale
///   無効（{1,1,1}固定）
/// </summary>
class YEmitterGroupGizmable : public IGizmable
{
public:
    explicit YEmitterGroupGizmable(YEmitterGroup* group)
        : group_(group)
    {
    }

    ~YEmitterGroupGizmable() override = default;

    // ── IGizmable 実装 ────────────────────────────────────────

    std::string GetGizmoLabel() const override {
        return "Group::" + group_->GetName();
    }

    // 位置：グループの SetPosition に委譲（全エミッターがオフセット維持で追従）
    Vector3 GetGizmoPosition() const override {
        return group_->GetPosition();
    }
    void SetGizmoPosition(const Vector3& pos) override {
        group_->SetPosition(pos);
    }

    // 回転：アダプタ内で保持し、差分をエミッターのオフセット位置に適用
    Vector3 GetGizmoRotation() const override {
        return rotation_;
    }
    void SetGizmoRotation(const Vector3& rot) override {
        Vector3 delta = {
            rot.x - rotation_.x,
            rot.y - rotation_.y,
            rot.z - rotation_.z
        };
        rotation_ = rot;
        ApplyRotationDelta(delta);
    }

    // スケール：無効
    Vector3 GetGizmoScale() const override {
        return { 1.0f, 1.0f, 1.0f };
    }
    void SetGizmoScale(const Vector3&) override { /* 無効 */ }

    void OnGizmoManipulationEnd() override { /* 必要なら自動保存など */ }

    float GetGizmoPickRadius() const override {
        return 1.2f;  // グループは大きめにして選択しやすく
    }

    // ── アダプタ固有 ─────────────────────────────────────────
    YEmitterGroup* GetGroup() const { return group_; }

private:
    /// グループ中心を軸に全エミッターのオフセット位置を回転させる
    /// 現在は Y 軸のみ対応（XZ はゲームによって不要なことが多い）
    void ApplyRotationDelta(const Vector3& delta)
    {
        if (std::abs(delta.y) < 1e-6f) return;

        const Vector3& center = group_->GetPosition();
        float sinY = std::sin(delta.y);
        float cosY = std::cos(delta.y);

        for (size_t i = 0; i < group_->GetEmitterCount(); ++i) {
            auto* e = group_->GetEmitter(i);
            if (!e) continue;

            Vector3 pos = e->GetPosition();
            float dx = pos.x - center.x;
            float dz = pos.z - center.z;

            e->SetPosition({
                center.x + dx * cosY - dz * sinY,
                pos.y,
                center.z + dx * sinY + dz * cosY
                });
        }
    }

    YEmitterGroup* group_ = nullptr;
    Vector3        rotation_ = {};
};

#endif // USE_IMGUI
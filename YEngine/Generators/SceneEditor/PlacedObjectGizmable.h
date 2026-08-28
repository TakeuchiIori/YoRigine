#pragma once

#ifdef USE_IMGUI

#include "Debugger/Gizmo/IGizmable.h"
#include <Object3D/ObjectManager.h>   // ObjectManager::PlacedObject
#include <string>

/// <summary>
/// ObjectManager::PlacedObject を IGizmable にラップするアダプター。
///
/// ■ 使い方
///   PlacedObjectGizmable gizmable(obj, objectManager);
///   gizmoController.DrawGizmo(camera, &gizmable, viewportPos, viewportSize);
///
/// ■ OnGizmoManipulationEnd() で ObjectManager::UpdateObjectTransform() を
///   自動呼び出しするため、呼び出し側でのトランスフォーム更新は不要。
/// </summary>
class PlacedObjectGizmable : public IGizmable
{
public:
    PlacedObjectGizmable(
        ObjectManager::PlacedObject* obj,
        ObjectManager* manager)
        : obj_(obj)
        , manager_(manager)
    {
    }

    //=================================================================
    // IGizmable 実装
    //=================================================================

    std::string GetGizmoLabel() const override
    {
        if (!obj_) return "(null)";
        return obj_->modelName + "##" + std::to_string(obj_->id);
    }

    Vector3 GetGizmoPosition() const override
    {
        if (!obj_) return {};
        return obj_->position;
    }

    void SetGizmoPosition(const Vector3& pos) override
    {
        if (!obj_) return;
        obj_->position = pos;
    }

    Vector3 GetGizmoRotation() const override
    {
        if (!obj_) return {};
        return obj_->rotation;     // ラジアン想定（ObjectManager 側に合わせること）
    }

    void SetGizmoRotation(const Vector3& rot) override
    {
        if (!obj_) return;
        obj_->rotation = rot;
    }

    Vector3 GetGizmoScale() const override
    {
        if (!obj_) return { 1,1,1 };
        return obj_->scale;
    }

    void SetGizmoScale(const Vector3& scale) override
    {
        if (!obj_) return;
        obj_->scale = scale;
    }

    /// ギズモ操作終了 → ObjectManager に変換を通知
    void OnGizmoManipulationEnd() override
    {
        if (obj_ && manager_)
            manager_->UpdateObjectTransform(*obj_);
    }

    /// ピック半径：スケールの最大値を基準に調整
    float GetGizmoPickRadius() const override
    {
        if (!obj_) return 0.5f;
        float s = std::max({ obj_->scale.x, obj_->scale.y, obj_->scale.z });
        return std::max(0.3f, s);
    }

    //=================================================================
    // 生ポインタ取得（外部から PlacedObject を参照したい場合）
    //=================================================================

    ObjectManager::PlacedObject* GetRawObject()  const { return obj_; }
    ObjectManager* GetManager()     const { return manager_; }

private:
    ObjectManager::PlacedObject* obj_ = nullptr;
    ObjectManager* manager_ = nullptr;
};

#endif // USE_IMGUI
#pragma once

#ifdef USE_IMGUI

#include "Vector3.h"
#include <string>

/// <summary>
/// ギズモで操作可能なオブジェクトが実装するインターフェース
/// GizmoController はこのインターフェースのみに依存するため、
/// YParticleEmitter / YEmitterGroup / 任意のゲームオブジェクト に同じギズモを使い回せる
/// </summary>
class IGizmable
{
public:
    virtual ~IGizmable() = default;

    /// ギズモ表示ラベル（デバッグ表示・重複ID防止に使う）
    virtual std::string GetGizmoLabel() const = 0;

    /// ワールド座標での位置（ギズモが直接読み書き）
    virtual Vector3  GetGizmoPosition() const = 0;
    virtual void     SetGizmoPosition(const Vector3& pos) = 0;

    /// 回転（オイラー角・ラジアン）
    virtual Vector3  GetGizmoRotation() const = 0;
    virtual void     SetGizmoRotation(const Vector3& rot) = 0;

    /// スケール
    virtual Vector3  GetGizmoScale() const = 0;
    virtual void     SetGizmoScale(const Vector3& scale) = 0;

    /// ギズモ操作終了通知（Undo 記録・JSON同期などをここで行う）
    virtual void     OnGizmoManipulationEnd() {}

    /// ビューポート内でのクリック判定に使う球の半径（ヒット判定用）
    virtual float    GetGizmoPickRadius() const { return 0.5f; }
};

#endif // USE_IMGUI
#pragma once
#ifdef USE_IMGUI

#include "Debugger/Gizmo/IGizmable.h"
#include "YParticleEmitter.h"

/// <summary>
/// YParticleEmitter を GizmoController で操作するためのアダプタ。
///
/// ■ 設計方針（なぜ直接継承しないか）
///   YParticleEmitter はゲームロジック側のクラス。
///   IGizmable は USE_IMGUI でしか存在しないエディタ側の概念。
///   直接継承するとゲームロジックにエディタ依存が混入するため
///   このアダプタが橋渡しをする。PlacedObjectGizmable と同じパターン。
///
/// ■ Position : emitter_->GetPosition / SetPosition に直接委譲
/// ■ Rotation : YParticleEmitter は回転を持たないためアダプタ内でローカル保持
///              将来 emitter に回転が追加されたらここだけ修正すればよい
/// ■ Scale    : エミッター形状はシェイプで制御するため無効（{1,1,1}固定）
/// </summary>
class YEmitterGizmable : public IGizmable
{
public:
    explicit YEmitterGizmable(YParticleEmitter* emitter, const std::string& label = "")
        : emitter_(emitter)
        , label_(label.empty() ? emitter->GetSystemName() : label)
    {
    }

    ~YEmitterGizmable() override = default;

    // ── IGizmable 実装 ────────────────────────────────────────

    std::string GetGizmoLabel() const override {
        return "Emitter::" + label_;
    }

    // 位置：エミッターに直接委譲
    Vector3 GetGizmoPosition() const override {
        return emitter_->GetPosition();
    }
    void SetGizmoPosition(const Vector3& pos) override {
        emitter_->SetPosition(pos);
    }

    // 回転：エミッターは持っていないのでアダプタ内で保持
    Vector3 GetGizmoRotation() const override {
        return rotation_;
    }
    void SetGizmoRotation(const Vector3& rot) override {
        rotation_ = rot;
        // 将来: emitter_->SetRotation(rot) のようにつなぐ
    }

    // スケール：無効
    Vector3 GetGizmoScale() const override {
        return { 1.0f, 1.0f, 1.0f };
    }
    void SetGizmoScale(const Vector3&) override { /* 無効 */ }

    void OnGizmoManipulationEnd() override { /* 必要なら自動保存など */ }

    float GetGizmoPickRadius() const override {
        return 0.8f;  // 少し大きめにして選択しやすく
    }

    // ── アダプタ固有 ─────────────────────────────────────────
    YParticleEmitter* GetEmitter() const { return emitter_; }

private:
    YParticleEmitter* emitter_ = nullptr;
    std::string       label_;
    Vector3           rotation_ = {};
};

#endif // USE_IMGUI
#pragma once

#include "YParticleManager.h"
#include "YParticleEmitter.h"
#include "Vector3.h"
#include <string>
#include <memory>

/// <summary>
/// エフェクトの生存管理ハンドル
///
/// ゲーム側から YParticle をワンライナーで再生・停止・追従できる軽量ラッパー。
/// スコープ抜けで自動停止しないため、明示的に Stop() を呼ぶか
/// IsAlive() を確認して寿命管理する。
///
/// 使い方（アクションゲーム典型例）:
///
///   // ヒットエフェクト（ワンショット）
///   EffectHandle hit = EffectHandle::PlayOneShot("HitSpark", hitPos);
///
///   // 継続エフェクト（武器スラッシュトレイル）
///   slashHandle_ = EffectHandle::Play("SlashTrail", swordPos, true);
///   slashHandle_.SetPosition(swordTipPos); // 毎フレーム追従
///   slashHandle_.Stop();                   // 攻撃終了時に停止
///
///   // 複数エフェクト（爆発 = 閃光 + 煙 + 破片）
///   EffectHandle::EmitAll({"Explosion","Smoke","Debris"}, blastPos, 30);
/// </summary>
class EffectHandle
{
public:
    EffectHandle() = default;
    ~EffectHandle() = default;

    // ── ファクトリメソッド ──────────────────────────────────────────────

    /// <summary>
    /// エフェクトを再生して EffectHandle を返す
    /// </summary>
    /// <param name="systemName">YParticleSystem 名</param>
    /// <param name="position">ワールド位置</param>
    /// <param name="loop">true = 手動 Stop() まで放出し続ける</param>
    /// <param name="emitCount">1 回あたり放出数（-1 = エミッタ設定に従う）</param>
    static EffectHandle Play(const std::string& systemName,
                             const Vector3&     position,
                             bool               loop      = false,
                             int                emitCount = -1);

    /// <summary>
    /// ワンショット（非ループ）エフェクトを簡単に再生
    /// </summary>
    static EffectHandle PlayOneShot(const std::string& systemName,
                                    const Vector3&     position,
                                    int                emitCount = 20);

    /// <summary>
    /// バースト（瞬間大量放出）
    /// </summary>
    static void Burst(const std::string& systemName,
                      const Vector3&     position,
                      int                count = 30);

    /// <summary>
    /// 複数システムを同位置で同時再生（爆発など）
    /// </summary>
    static void EmitAll(std::initializer_list<const char*> systemNames,
                        const Vector3& position,
                        int            countEach = 20);

    // ── 操作 ────────────────────────────────────────────────────────────

    void SetPosition(const Vector3& pos);
    void Stop();

    // ── クエリ ──────────────────────────────────────────────────────────

    bool IsValid() const { return emitter_ != nullptr; }
    bool IsActive() const;
    const std::string& GetSystemName() const { return systemName_; }

    // ── 内部向けアクセス ────────────────────────────────────────────────
    YParticleEmitter* GetEmitter() const { return emitter_.get(); }

private:
    std::string                      systemName_;
    std::shared_ptr<YParticleEmitter> emitter_;
};

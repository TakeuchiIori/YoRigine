#pragma once
// ===========================================================
// GpuParticleHandle.h
//
// EffectHandle / YVfxHandle と同じ発想で、GPUパーティクル（GpuEmitManager
// のエミッターグループ）をゲームコードからワンライナーで再生・操作するハンドル。
//
// 「名前」は GpuEmitManager のグループ名（JSON の groupName）。
//
// 使い方:
//   // ワンショット（1回発生して粒子は寿命で自然消滅。破片・火花など）
//   GpuParticleHandle::PlayOneShot("ExplosionDebris", blastPos);
//
//   // 継続再生（ループ。手動 Stop() まで発生し続ける。オーラ・炎など）
//   auraHandle_ = GpuParticleHandle::Play("Aura", playerPos);
//   auraHandle_.SetPosition(playerPos); // 毎フレーム追従
//   auraHandle_.Stop();                 // 停止（既存粒子は寿命で自然に消える）
// ===========================================================
#include "Vector3.h"
#include <string>

class GpuParticleHandle
{
public:
    GpuParticleHandle() = default;
    ~GpuParticleHandle() = default;

    // ── ファクトリ ────────────────────────────────────────────────────

    // 継続再生（ループ）。返り値のハンドルで追従・停止する。
    static GpuParticleHandle Play(const std::string& groupName,
                                  const Vector3&     position);

    // ワンショット。1回だけ発生し、粒子は寿命で自然消滅する。
    // countPerEmitter < 0 で各エミッタの設定値をそのまま使う。
    static void PlayOneShot(const std::string& groupName,
                            const Vector3&     position,
                            float              countPerEmitter = -1.0f);

    // ── 操作 ─────────────────────────────────────────────────────────

    void SetPosition(const Vector3& pos);
    void Stop();

    // ── クエリ ───────────────────────────────────────────────────────

    bool IsValid() const { return valid_; }
    bool IsActive() const;
    const std::string& GetGroupName() const { return groupName_; }

private:
    std::string groupName_;
    bool        valid_ = false;
};

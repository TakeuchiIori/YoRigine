#pragma once
// ===========================================================
// VfxMeshHandle.h
//
// EffectHandle と同じ発想で VfxMesh 系エフェクトを
// ゲームコードからワンライナーで再生・操作するハンドル。
//
// 使い方:
//   // ワンショット爆発（位置・スケール指定）
//   VfxMeshHandle::PlayOneShot("Explosion", blastPos, 1.5f);
//
//   // ループ稲妻（始点→終点、毎フレーム追従）
//   boltHandle_ = VfxMeshHandle::Play("Lightning", startPos, endPos);
//   boltHandle_.SetEndpoints(swordBase, swordTip);
//   boltHandle_.Stop();
//
//   // スモーク（位置指定、スケール1.0）
//   smokeHandle_ = VfxMeshHandle::Play("Smoke", hitPos, 2.0f, true);
//   smokeHandle_.SetPosition(newPos);
// ===========================================================
#include "Vector3.h"
#include <string>
#include <cstdint>

class VfxMeshHandle
{
public:
    VfxMeshHandle() = default;
    ~VfxMeshHandle() = default;

    // ── ファクトリ ────────────────────────────────────────────────────

    // 位置 + スケール指定（スモーク・衝撃波など球系エフェクト向け）
    static VfxMeshHandle Play(const std::string& assetName,
                              const Vector3&     position,
                              float              scale = 1.0f,
                              bool               loop  = false);

    static void PlayOneShot(const std::string& assetName,
                            const Vector3&     position,
                            float              scale = 1.0f);

    // 始点・終点指定（稲妻など方向性エフェクト向け）
    static VfxMeshHandle PlayBolt(const std::string& assetName,
                                  const Vector3&     start,
                                  const Vector3&     end,
                                  bool               loop = false);

    // ── 操作 ─────────────────────────────────────────────────────────

    void SetPosition(const Vector3& pos);
    void SetScale(float scale);
    void SetEndpoints(const Vector3& start, const Vector3& end);
    void Stop();

    // ── クエリ ───────────────────────────────────────────────────────

    bool IsValid() const { return id_ != 0; }
    bool IsAlive() const;

private:
    VfxMeshHandle(uint32_t id) : id_(id) {}

    uint32_t    id_ = 0;

    friend class VfxMeshSpawner;
};

#pragma once
// ===========================================================
// VfxMeshSpawner.h
//
// VfxMesh 系エフェクト（Lightning / Shockwave / Smoke）を
// 名前とトランスフォームで生成・管理するシングルトン。
//
// 使い方（MyGame 側）:
//   // Initialize
//   VfxMeshSpawner::GetInstance()->Initialize();
//   VfxMeshSpawner::GetInstance()->ScanDirectory("Resources/Vfx/");
//
//   // 毎フレーム
//   VfxMeshSpawner::GetInstance()->SetCamera(camera);
//   VfxMeshSpawner::GetInstance()->Update(dt);
//   VfxMeshSpawner::GetInstance()->Draw();
// ===========================================================
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Effects/LightningMesh.h>
#include <Vfx/VfxMesh/Effects/LightVolumeMesh.h>
#include <Vfx/VfxMesh/Effects/VolumeSmokeMesh.h>
#include <Vfx/VfxMesh/Effects/ShockwaveMesh.h>
#include "Vector3.h"
#include "Memory/PoolAllocator.h"

class Camera;

namespace YoRigine { class DirectXCommon; }

class VfxMeshSpawner
{
public:
    static VfxMeshSpawner* GetInstance();

    void Initialize();
    void Finalize();

    void SetCamera(Camera* camera) { camera_ = camera; }

    // アセット登録
    void LoadAsset(const std::string& filePath);
    void ScanDirectory(const std::string& dir = "Resources/Json/VfxMesh/");

    // ロード済みアセット名の一覧（Compositeエディタのドロップダウン用）
    std::vector<std::string> GetAssetNames() const;

    // 毎フレーム
    void Update(float deltaTime);
    void Draw();

    // ── 生成 ─────────────────────────────────────────────────────────
    // 位置 + スケール（スモーク・衝撃波など）
    uint32_t Spawn(const std::string& assetName,
                   const Vector3&     position,
                   float              scale = 1.0f,
                   bool               loop  = false);

    // 始点・終点（稲妻など方向性エフェクト）
    uint32_t SpawnBolt(const std::string& assetName,
                       const Vector3&     start,
                       const Vector3&     end,
                       bool               loop = false);

    // ── 操作（VfxMeshHandle から呼ばれる） ─────────────────────────
    void SetPosition(uint32_t id, const Vector3& pos);
    void SetScale(uint32_t id, float scale);
    void SetEndpoints(uint32_t id, const Vector3& start, const Vector3& end);
    void Stop(uint32_t id);
    bool IsAlive(uint32_t id) const;

private:
    VfxMeshSpawner()  = default;
    ~VfxMeshSpawner() = default;
    VfxMeshSpawner(const VfxMeshSpawner&)            = delete;
    VfxMeshSpawner& operator=(const VfxMeshSpawner&) = delete;

    // 同時に存在できる VfxMesh エフェクトの上限（PoolAllocator の固定長）
    static constexpr size_t kMaxActiveVfx = 256;

    // ── サブ効果1個分の実行時リソース ────────────────────────────────
    // アセットの subEffects と1対1対応（同じ種類が複数あればその数だけ作る）
    struct SubEffectRT
    {
        const YoRigine::VfxSubEffect* def = nullptr;   // アセット内の定義を指す

        // def->type に対応するものだけ生成される
        std::unique_ptr<YoRigine::VolumeSmokeMesh> smoke;
        std::unique_ptr<YoRigine::LightningMesh>   lightning;
        std::unique_ptr<YoRigine::ShockwaveMesh>   shockwave;
        std::unique_ptr<YoRigine::LightVolumeMesh> lightVolume;

        // CB リソース（インスタンス独立。実型は def->type で決まる）
        Microsoft::WRL::ComPtr<ID3D12Resource> cbRes;
        void*                                  cbMapped = nullptr;

        // Smoke の CB 用読み戻し値
        Vector3 smokeCenter = { 0.f, 0.f, 0.f };
        float   smokeRadius = 1.5f;

        // モーション評価結果（Update で書き込み、Draw の CB 反映で使う）
        Vector4 tint            = { 1.f, 1.f, 1.f, 1.f }; // rgb=色乗算 / a=不透明度乗算
        float   beamRadiusScale = 1.f;
        float   beamGlowScale   = 1.f;
        bool    visible         = true;                   // Visibility モーションの結果
    };

    // ── アクティブエフェクト ─────────────────────────────────────────
    struct ActiveEffect
    {
        uint32_t id   = 0;
        bool     alive = true;
        bool     loop  = false;
        float    age   = 0.f;

        // アセットはコピーせず assetMap_ の実体を指す（生成時のヒープ確保/コピーを回避）
        const YoRigine::VfxEffectAsset* asset = nullptr;

        // 位置・スケール
        Vector3 position  = { 0.f, 0.f, 0.f };
        float   scale     = 1.0f;
        Vector3 boltStart = { 0.f, 0.f, 0.f };
        Vector3 boltEnd   = { 0.f, 3.f, 0.f };
        // SetEndpoints/SpawnBolt で明示指定されたか。
        // false なら Lightning はアセットの direction/length から端点を自動計算する。
        bool    explicitEndpoints = false;

        // バースト進捗（-1=ループ継続、0..1=ワンショット）
        float burstProgress = -1.f;

        // サブ効果ごとの実行時リソース
        std::vector<SubEffectRT> subs;

        void Release();

        // プール返却時（~ActiveEffect）にマップ済み CB を確実に Unmap する
        ~ActiveEffect() { Release(); }
    };

    void InitEffect(ActiveEffect& fx);
    void UpdateEffect(ActiveEffect& fx, float dt);
    void DrawEffect(ActiveEffect& fx);

    YoRigine::DirectXCommon* dxCommon_ = nullptr;
    Camera*  camera_  = nullptr;
    uint32_t nextId_  = 1;

    std::unordered_map<std::string, YoRigine::VfxEffectAsset> assetMap_;

    // 実体は固定長プールから確保（生成/破棄でヒープが増減しない）。
    // active_ は「今生きているエフェクトへのポインタ列」（reserve 済みで再確保しない）。
    PoolAllocator<ActiveEffect, kMaxActiveVfx> pool_;
    std::vector<ActiveEffect*>                 active_;
};

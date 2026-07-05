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

#include "VfxEffectAsset.h"
#include "LightningMesh.h"
#include "ShockwaveMesh.h"
#include "VolumeSmokeMesh.h"
#include "Vector3.h"

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

    // ── アクティブエフェクト ─────────────────────────────────────────
    struct ActiveEffect
    {
        uint32_t id   = 0;
        bool     alive = true;
        bool     loop  = false;
        float    age   = 0.f;

        YoRigine::VfxEffectAsset asset;

        // 位置・スケール
        Vector3 position  = { 0.f, 0.f, 0.f };
        float   scale     = 1.0f;
        Vector3 boltStart = { 0.f, 0.f, 0.f };
        Vector3 boltEnd   = { 0.f, 3.f, 0.f };

        // バースト進捗（-1=ループ継続、0..1=ワンショット）
        float burstProgress = -1.f;
        Vector3 smokeCenter;
        float   smokeRadius = 1.5f;

        // メッシュ実体
        std::unique_ptr<YoRigine::VolumeSmokeMesh> smoke;
        std::unique_ptr<YoRigine::LightningMesh>   lightning;
        std::unique_ptr<YoRigine::ShockwaveMesh>   shockwave;

        // CB リソース（各エフェクト独立）
        Microsoft::WRL::ComPtr<ID3D12Resource> smokeCBRes;
        YoRigine::SmokeParamsCB*               smokeCBMapped    = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> lightningCBRes;
        YoRigine::LightningParamsCB*           lightningCBMapped = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> shockwaveCBRes;
        YoRigine::ShockwaveParamsCB*           shockwaveCBMapped = nullptr;

        void Release();
    };

    void InitEffect(ActiveEffect& fx);
    void UpdateEffect(ActiveEffect& fx, float dt);
    void DrawEffect(ActiveEffect& fx);

    YoRigine::DirectXCommon* dxCommon_ = nullptr;
    Camera*  camera_  = nullptr;
    uint32_t nextId_  = 1;

    std::unordered_map<std::string, YoRigine::VfxEffectAsset> assetMap_;
    std::vector<std::unique_ptr<ActiveEffect>>                 effects_;
};

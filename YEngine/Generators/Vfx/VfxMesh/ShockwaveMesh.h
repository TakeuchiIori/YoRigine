#pragma once
// ===========================================================
// ShockwaveMesh.h
//
// 爆発の衝撃波リング。カメラを向くクワッドを 1 枚生成し、
// シェーダ側で「中心から外へ広がって消えるリング」をアニメする。
// 加算ブレンドで HDR → Bloom が乗る。
//
//   shock_ = std::make_unique<ShockwaveMesh>();
//   shock_->Initialize();
//   shock_->SetCamera(camera);
//   shock_->SetTransform(center, radius);
//   shock_->Update(dt);
//   // 呼び出し元で PSO("VfxMeshShockwave") + gCamera + gMeshParam(ShockwaveParamsCB)
//   shock_->Draw(cmdList);
// ===========================================================
#include "ProceduralMeshBase.h"
#include "VfxEffectAsset.h"   // ShockwaveEffectParam

class Camera;

namespace YoRigine {

// シェーダ b1: gMeshParam（VfxMesh_Common.hlsli の ShockwaveParams と一致）
struct ShockwaveParamsCB
{
    Vector4 color;     // 色（rgb>1 で Bloom）
    float   time;      // アニメ時間
    float   duration;  // 1サイクルの秒数（膨張→消滅）
    float   thickness; // リングの太さ(0..1)
    float   burst;     // -1=継続ループ, 0..1=爆発ワンショット進捗
};

class ShockwaveMesh : public ProceduralMeshBase
{
public:
    ShockwaveMesh()  = default;
    ~ShockwaveMesh() override = default;

    void Initialize();

    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetTransform(const Vector3& center, float radius);
    void ApplyParam(const ShockwaveEffectParam& param) { param_ = param; }

    void Update(float deltaTime) override;
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

private:
    void RebuildVertices();

    ShockwaveEffectParam param_;
    Camera* camera_ = nullptr;

    Vector3 center_ = { 0.f, 0.f, 0.f };
    float   radius_ = 3.0f;

    std::vector<ProceduralMeshVertex> vertices_;
    float time_ = 0.f;
};

} // namespace YoRigine

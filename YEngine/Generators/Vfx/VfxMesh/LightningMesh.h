#pragma once
// ===========================================================
// LightningMesh.h
//
// プロシージャルな稲妻（雷）。始点→終点を midpoint displacement で
// ジグザグに折り、カメラを向くリボンとして描画する。
// 一定間隔で経路を再生成して明滅・パチパチ感を出す。加算ブレンド前提。
//
// 使い方:
//   bolt_ = std::make_unique<LightningMesh>();
//   bolt_->Initialize();
//   bolt_->SetCamera(camera);
//   bolt_->SetEndpoints(start, end);
//   bolt_->Update(dt);
//   // 呼び出し元で PSO("VfxMeshLightning") + gCamera + gMeshParam(LightningParamsCB) を bind
//   bolt_->Draw(cmdList);
// ===========================================================
#include "ProceduralMeshBase.h"
#include "VfxEffectAsset.h"   // LightningEffectParam

class Camera;

namespace YoRigine {

// シェーダ b1: gMeshParam（VfxMesh_Common.hlsli の LightningParams と一致）
struct LightningParamsCB
{
    Vector4 color;            // 芯(コア)の色（HDR）
    Vector4 glowColor;        // 外側グローの色（HDR）★2色
    Vector4 branchColor;      // 枝の色（HDR）
    float   time;             // アニメ時間
    float   glowPower;        // 中心グロー強度（芯の細さ）
    float   coreWidth;        // 芯の太さ/実体感(0..1)
    float   solidness;        // 透明感を減らす(0..1)
    float   outlineIntensity; // 縁/枝のアウトライン強調
    float   _pad0;
    float   _pad1;
    float   _pad2;
};

class LightningMesh : public ProceduralMeshBase
{
public:
    LightningMesh()  = default;
    ~LightningMesh() override = default;

    void Initialize();

    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetEndpoints(const Vector3& start, const Vector3& end);
    void ApplyParam(const LightningEffectParam& param) { param_ = param; }

    void Update(float deltaTime) override;
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

private:
    void RebuildVertices();
    // start→end を midpoint displacement で折る（再帰）。点列を out へ追加。
    void Subdivide(const Vector3& a, const Vector3& b, float amp, int depth, std::vector<Vector3>& out);
    // 1 本のジグザグ折れ線をカメラ向きリボンとして vertices_ に積む
    void AppendBoltRibbon(const std::vector<Vector3>& pts, float width, const Vector4& color);

    LightningEffectParam param_;
    Camera* camera_ = nullptr;

    Vector3 start_ = { 0.f, 0.f, 0.f };
    Vector3 end_   = { 0.f, 3.f, 0.f };

    std::vector<ProceduralMeshVertex> vertices_;
    float time_        = 0.f;
    float flickerTimer_ = 0.f;
    uint32_t seed_        = 1; // 形生成中の作業シード（毎フレーム flickerSeed_ から復元）
    uint32_t flickerSeed_ = 1; // フリッカーで変わる基準シード（これが変わった時だけ形が変わる）
};

} // namespace YoRigine

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
#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>

class Camera;

namespace YoRigine {

// シェーダ b1: gMeshParam（VfxMesh_Common.hlsli の ShockwaveParams と一致）
struct ShockwaveParamsCB
{
    Vector4 color;      // 色（rgb>1 で Bloom / a=不透明度。モジュール適用済み）
    float   thickness;  // リングの太さ(UV)
    float   ringRadius; // リング位置(UV, 0..1)。膨張は Module(scale) が担当
    float   _pad0;
    float   _pad1;
};

class ShockwaveMesh : public ProceduralMeshBase
{
public:
    ShockwaveMesh()  = default;
    ~ShockwaveMesh() override = default;

    // ── 初期化 ───────────────────────────────────────────────

    /// 頂点バッファを確保する。使用前に必ず呼ぶ
    void Initialize();

    // ── 設定 ─────────────────────────────────────────────────

    /// 描画に使うカメラを渡す（カメラ向きクワッドの計算に必要）
    void SetCamera(Camera* camera) { camera_ = camera; }

    /// 衝撃波リングの中心とワールド半径を設定する
    void SetTransform(const Vector3& center, float radius);

    /// パラメータを差し替える（エディタのホットリロード用）
    void ApplyParam(const ShockwaveEffectParam& param) { param_ = param; }

    // ── 毎フレーム ────────────────────────────────────────────

    /// VfxEvalState の位置・スケールから中心と半径を反映する
    void Drive(const VfxEvalState& state) override { SetTransform(state.position, param_.radius * state.scale); }

    /// time を進め頂点を更新する
    void Update(float deltaTime) override;

    /// 頂点バッファをバインドして描画する
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    // ── レンダリングインターフェース ──────────────────────────

    const char* GetPSOName()    const override { return "VfxMeshShockwave"; }
    size_t      GetCBByteSize() const override;

    /// モジュール評価結果を定数バッファに書き込む
    void        FillCB(void* mapped, const CBFillArgs& args) const override;

    /// VfxMeshRegistry に渡す記述子を返す
    static VfxElementDesc Describe();

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

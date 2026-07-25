#pragma once
// ===========================================================
// LightVolumeMesh.h
//
// OBB 形状の光のボリュームを表現するメッシュ
// ワールド行列 (位置・回転・スケール) を渡すと
// OBB に対応した箱ポリゴンを動的生成して描画する
//
// 使い方:
//   vol_ = std::make_unique<LightVolumeMesh>();
//   vol_->Initialize(editor_->GetAsset().lightVolume);
//
//   // 毎フレーム
//   vol_->SetTransform(position, rotation);  // OBB の姿勢
//   vol_->Update(deltaTime);
//   vol_->Draw(cmdList);
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>

namespace YoRigine {

// シェーダ b1: gMeshParam（VfxMesh_Volume.hlsli の LightVolumeParams と一致）。
// Editor / ランタイム Spawner の両方から使うので Mesh 側ヘッダに置く。
struct LightVolumeParamsCB
{
    float color[4];
    float edgeFade;
    float depthFade;
    float noiseTiling;
    float noiseStrength;
    float time;
    float beamStrength;
    float beamRadius;
    float beamPower;
    float beamGlow;
};

class LightVolumeMesh : public ProceduralMeshBase
{
public:
    LightVolumeMesh()  = default;
    ~LightVolumeMesh() override = default;

    // --------------------------------------------------------
    // 初期化 / パラメータ更新
    // --------------------------------------------------------

    // ── 初期化 ───────────────────────────────────────────────

    /// パラメータを渡して初期化する。頂点バッファを確保し最初の OBB を構築する
    void Initialize(const LightVolumeEffectParam& param);

    /// パラメータを差し替える（エディタのホットリロード用）。次の Update で再構築される
    void ApplyParam(const LightVolumeEffectParam& param);

    // ── 毎フレーム ────────────────────────────────────────────

    /// OBB のワールド姿勢を3軸ベクトルで更新する（姿勢が変わった時だけ頂点を再構築）
    void SetTransform(const Vector3& center,
                      const Vector3& right,
                      const Vector3& up,
                      const Vector3& forward);

    /// SetTransform の簡易版。center + Y 軸回転角（ラジアン）だけ指定する
    void SetTransform(const Vector3& center, float yawRad);

    /// time を進め、dirty なら頂点を再構築する
    void Update(float deltaTime) override;

    /// 頂点バッファをバインドして描画する
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    const char* GetPSOName()    const override { return "VfxMeshVolume"; }
    size_t      GetCBByteSize() const override;

    /// モジュール評価結果を定数バッファに書き込む
    void        FillCB(void* mapped, const CBFillArgs& args) const override;

    // ── アクセサ ─────────────────────────────────────────────

    const LightVolumeEffectParam& GetParam() const { return param_; }

    /// VfxMeshRegistry に渡す記述子を返す
    static VfxElementDesc Describe();

private:
    // OBB の 6 面 (各面 = 2 三角形) を生成
    void RebuildVertices();

    // 1 面分 (4 隅 → 6 頂点) を vertices_ に追加するヘルパー
    // @param corners  面の 4 隅ワールド座標 (時計回り)
    // @param uvZ      Z 方向の正規化距離 (age として渡す)
    void AppendFace(const Vector3 corners[4],
                    const Vector4& color,
                    float          uvZ);

private:
    LightVolumeEffectParam param_;

    // OBB 姿勢
    Vector3 center_  = { 0.f, 0.f, 0.f };
    Vector3 right_   = { 1.f, 0.f, 0.f };
    Vector3 up_      = { 0.f, 1.f, 0.f };
    Vector3 forward_ = { 0.f, 0.f, 1.f };

    // CPU 側頂点バッファ
    std::vector<ProceduralMeshVertex> vertices_;

    // 姿勢が変わったときだけ再構築する
    bool dirty_ = true;

    float time_ = 0.f;
};

} // namespace YoRigine

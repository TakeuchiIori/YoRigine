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

namespace YoRigine {

class LightVolumeMesh : public ProceduralMeshBase
{
public:
    LightVolumeMesh()  = default;
    ~LightVolumeMesh() override = default;

    // --------------------------------------------------------
    // 初期化 / パラメータ更新
    // --------------------------------------------------------

    void Initialize(const LightVolumeEffectParam& param);

    /// ホットリロード対応: パラメータを差し替え → 次の Update で再構築
    void ApplyParam(const LightVolumeEffectParam& param);

    // --------------------------------------------------------
    // 毎フレーム
    // --------------------------------------------------------

    /// OBB のワールド変換を更新する
    /// @param center   OBB 中心ワールド座標
    /// @param right    X 軸方向 (正規化)
    /// @param up       Y 軸方向 (正規化)
    /// @param forward  Z 軸方向 (正規化)
    void SetTransform(const Vector3& center,
                      const Vector3& right,
                      const Vector3& up,
                      const Vector3& forward);

    /// SetTransform の簡易版: center + Y 軸回転のみ
    void SetTransform(const Vector3& center, float yawRad);

    void Update(float deltaTime) override;
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    // --------------------------------------------------------
    // アクセサ
    // --------------------------------------------------------
    const LightVolumeEffectParam& GetParam() const { return param_; }

private:
    // OBB の 6 面 (各面 = 2 三角形 = 4 頂点 + 縮退) を生成
    void RebuildVertices();

    // 1 面分 (4 頂点) を vertices_ に追加するヘルパー
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

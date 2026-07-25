#pragma once
// ===========================================================
// VolumeSmokeMesh.h
//
// 球メッシュ＋ノイズ(FBM)アニメシェーダで、Valorant Omen のような
// めらめら渦巻くボリューム風スモーク/エネルギー球を表現する。
//
// 形状はワールド空間の UV 球を動的生成（VfxMesh.VS はワールド頂点前提）。
// 渦巻きアニメはシェーダ側 (time) で行うので、姿勢が変わらなければ
// 頂点は再構築しない（LightVolumeMesh と同じ方針）。
//
// 使い方:
//   smoke_ = std::make_unique<VolumeSmokeMesh>();
//   smoke_->Initialize();
//   smoke_->SetTransform(center, radius);  // 毎フレーム or 変化時
//   smoke_->Update(dt);
//   // 呼び出し元で PSO("VfxMeshSmoke") + gCamera + gMeshParam(SmokeParamsCB) を bind
//   smoke_->Draw(cmdList);
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>

namespace YoRigine {

// シェーダ b1: gMeshParam に渡す CB レイアウト（VfxMesh_Common.hlsli の SmokeParams と一致）
struct SmokeParamsCB
{
    Vector4 color;          // rgb=色(>1でBloom), a=密度の基準
    Vector3 center;         // 球中心(ワールド) — PSでフレネル法線に使用
    float   radius;         // 球半径
    float   time;           // アニメ時間
    float   noiseScale;     // FBM タイリング
    float   noiseStrength;  // 渦巻き(揺らぎ)の強さ 0..1
    float   scrollSpeed;    // ノイズスクロール速度
    float   fresnelPower;   // 縁の柔らかさ（大きいほど縁が薄い）
    float   density;        // 全体の不透明度倍率
    float   noiseOctaves;   // FBM オクターブ数(1-4, float で渡す)
    float   rimIntensity;   // リム発光強度（太陽フレア風 / Bloom 用）
    float   burst;          // -1=継続, 0..1=爆発ワンショット進捗
    float   _pad2[3];
    Vector4 smokeColor;     // 爆発後に遷移する煙色
};

class VolumeSmokeMesh : public ProceduralMeshBase
{
public:
    VolumeSmokeMesh()  = default;
    ~VolumeSmokeMesh() override = default;

    // ── 初期化 ───────────────────────────────────────────────

    /// UV 球メッシュを生成して頂点バッファを確保する
    void Initialize(int rings = 18, int sectors = 28);

    // ── 設定 ─────────────────────────────────────────────────

    /// 球の中心・半径を設定する（変化があった時だけ頂点を再構築）
    void SetTransform(const Vector3& center, float radius);

    /// 頂点色を更新する（実際の見た目は CB の SmokeParams.color で制御するため通常は白のまま）
    void SetColor(const Vector4& color) { color_ = color; dirty_ = true; }

    /// パラメータを差し替える（エディタのホットリロード用）
    void ApplyParam(const SmokeEffectParam& param) { param_ = param; }

    // ── 毎フレーム ────────────────────────────────────────────

    /// time を進め、dirty なら球頂点を再構築する
    void Update(float deltaTime) override;

    /// VfxEvalState から位置・スケール・バースト進捗を反映する
    void Drive(const VfxEvalState& state) override;

    /// 頂点バッファをバインドして描画する
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    // ── レンダリングインターフェース ──────────────────────────

    const char* GetPSOName()    const override { return "VfxMeshSmoke"; }
    size_t      GetCBByteSize() const override;

    /// モジュール評価結果を定数バッファに書き込む
    void        FillCB(void* mapped, const CBFillArgs& args) const override;

    // ── アクセサ ─────────────────────────────────────────────

    const Vector3& GetCenter() const { return center_; }
    float          GetRadius() const { return radius_; }

    /// VfxMeshRegistry に渡す記述子を返す
    static VfxElementDesc Describe();

private:
    void RebuildVertices();

    SmokeEffectParam param_;   // 半径/上昇速度など（Drive で参照）
    int     rings_   = 18;
    int     sectors_ = 28;
    Vector3 center_  = { 0.f, 0.f, 0.f };
    float   radius_  = 1.5f;
    Vector4 color_   = { 1.0f, 1.0f, 1.0f, 1.0f }; // 頂点色は白。実際の色は CB(SmokeParams.color) で駆動

    std::vector<ProceduralMeshVertex> vertices_;
    bool  dirty_ = true;
    float time_  = 0.f;
};

} // namespace YoRigine

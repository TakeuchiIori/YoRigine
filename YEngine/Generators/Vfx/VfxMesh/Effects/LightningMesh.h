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
#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>

namespace YoRigine { class Camera; }

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

    // ── 初期化 ───────────────────────────────────────────────

    /// 頂点バッファを確保する。使用前に必ず呼ぶ
    void Initialize();

    // ── 設定 ─────────────────────────────────────────────────

    /// 描画に使うカメラを渡す（カメラ向きリボンの計算に必要）
    void SetCamera(YoRigine::Camera* camera) { camera_ = camera; }

    /// 稲妻の始点・終点をワールド座標で設定する
    void SetEndpoints(const Vector3& start, const Vector3& end);

    /// パラメータを差し替える（エディタのホットリロード用）
    void ApplyParam(const LightningEffectParam& param) { param_ = param; }

    // ── 毎フレーム ────────────────────────────────────────────

    /// VfxEvalState から端点を更新する（useAutoEndpoints=true なら direction/length から自動計算）
    void Drive(const VfxEvalState& state) override;

    /// フリッカータイマーを進め、必要なら経路を再生成してリボンを更新する
    void Update(float deltaTime) override;

    /// 頂点バッファをバインドして描画する
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    // ── レンダリングインターフェース ──────────────────────────

    const char* GetPSOName()    const override { return "VfxMeshLightning"; }
    size_t      GetCBByteSize() const override;

    /// モジュール評価結果（tint・age・burst）を定数バッファに書き込む
    void        FillCB(void* mapped, const CBFillArgs& args) const override;

    // ── レジストリ登録 ────────────────────────────────────────

    /// VfxMeshRegistry に渡す記述子を返す（Spawner の Initialize で1行追加するだけでよい）
    static VfxElementDesc Describe();

private:
    // ── 内部処理 ─────────────────────────────────────────────

    /// フリッカー時のみ実行 - Subdivide で経路を生成し cachedPaths_ にキャッシュする
    void RebuildPaths();

    /// 毎フレーム実行 - キャッシュ済み経路からカメラ向きリボンを展開してアップロードする
    void RebuildRibbons();

    /// a→b を midpoint displacement で再帰的に折り、制御点を out に追加する
    void Subdivide(const Vector3& a, const Vector3& b, float amp, int depth, std::vector<Vector3>& out);

    /// 1本分の制御点列をカメラ向き三角形リボンに変換して vertices_ に積む
    void AppendBoltRibbon(const std::vector<Vector3>& pts, float width, const Vector4& color);

    // ── データ ───────────────────────────────────────────────

    LightningEffectParam param_;
    YoRigine::Camera* camera_ = nullptr;

    Vector3 start_ = { 0.f, 0.f, 0.f };
    Vector3 end_   = { 0.f, 3.f, 0.f };

    /// フリッカー時のみ再生成する経路キャッシュ（本線 + 枝）
    struct BoltPath {
        std::vector<Vector3> points;
        Vector4              color;
        float                width = 0.f;
    };
    std::vector<BoltPath> cachedPaths_;

    uint32_t lastBuiltSeed_  = 0;    ///< 直前の RebuildPaths 時点の flickerSeed_
    bool     endpointsDirty_ = true; ///< 端点変化時に RebuildPaths を強制する

    std::vector<ProceduralMeshVertex> vertices_;
    float    time_         = 0.f;
    float    flickerTimer_ = 0.f;
    uint32_t seed_         = 1; ///< Subdivide 中の作業シード（毎回 flickerSeed_ から復元）
    uint32_t flickerSeed_  = 1; ///< フリッカーで変わる基準シード
};

} // namespace YoRigine

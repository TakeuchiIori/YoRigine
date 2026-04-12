#pragma once
// ===========================================================
// TrailMesh.h
//
// 剣閃・残像トレイルを板ポリゴン列で表現する
//
// 使い方:
//   // 初期化
//   trail_ = std::make_unique<TrailMesh>();
//   trail_->Initialize(editor_->GetAsset().trail);
//
//   // 毎フレーム: 先端と根元のワールド座標を渡す
//   trail_->AddPoint(tipPos, rootPos);
//   trail_->Update(deltaTime);       // 寿命管理・頂点生成
//   trail_->Draw(cmdList);           // GPU 描画
// ===========================================================
#include "ProceduralMeshBase.h"
#include "VfxEffectAsset.h"
#include <deque>

namespace YoRigine {

// -----------------------------------------------------------
// トレイル1点分の履歴
// -----------------------------------------------------------
struct TrailPoint
{
    Vector3 tip;    // 刃先ワールド座標
    Vector3 root;   // 根本ワールド座標
    float   age;    // 経過時間 (秒)
};

// -----------------------------------------------------------
class TrailMesh : public ProceduralMeshBase
{
public:
    TrailMesh()  = default;
    ~TrailMesh() override = default;

    // --------------------------------------------------------
    // 初期化 / リセット
    // --------------------------------------------------------

    /// VfxEffectAsset の TrailEffectParam を使って初期化する
    void Initialize(const TrailEffectParam& param);

    /// パラメータだけ差し替える (ホットリロード対応)
    void ApplyParam(const TrailEffectParam& param);

    /// ポイント履歴をすべてクリア
    void Clear();

    // --------------------------------------------------------
    // 毎フレーム
    // --------------------------------------------------------

    /// ポイントを先頭に追加する
    /// @param tip   刃先ワールド座標
    /// @param root  根本ワールド座標
    void AddPoint(const Vector3& tip, const Vector3& root);

    /// 寿命切れポイントの削除 + 頂点バッファ再構築
    void Update(float deltaTime) override;

    /// 描画コマンド発行
    /// b0=Camera CBV, b1=MeshTrailParams CBV は呼び出し元でセット済みを想定
    void Draw(ID3D12GraphicsCommandList* cmdList) override;

    // --------------------------------------------------------
    // アクセサ
    // --------------------------------------------------------
    size_t GetPointCount() const { return points_.size(); }
    const TrailEffectParam& GetParam() const { return param_; }

private:
    // --------------------------------------------------------
    // 内部処理
    // --------------------------------------------------------

    /// points_ から ProceduralMeshVertex のストリップを生成
    void RebuildVertices();

    /// age を [0,1] に正規化 (0=新しい端, 1=古い端)
    float NormalizeAge(float age) const
    {
        return (param_.lifetime > 0.f) ? std::min(age / param_.lifetime, 1.f) : 1.f;
    }

private:
    TrailEffectParam param_;

    // 形状パラメータ
    TrailShapeType shapeType_    = TrailShapeType::Flat;
    int            widthSegments_ = 1;
    float          arcAngleDeg_  = 120.0f;

    // ポイント履歴 (front = 最新, back = 最古)
    std::deque<TrailPoint> points_;

    // 頂点バッファの CPU 側コピー
    std::vector<ProceduralMeshVertex> vertices_;

    // 累積時間 (UV スクロール用)
    float time_ = 0.f;
};

} // namespace YoRigine

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
        Vector3 tip;      // 刃先ワールド座標
        Vector3 root;     // 根本ワールド座標
        Vector3 widthDir; // 幅方向の単位ベクトル (Arc/Fan 用)
        float   age;      // 経過時間 (秒)
    };

    // -----------------------------------------------------------
    class TrailMesh : public ProceduralMeshBase
    {
    public:
        TrailMesh() = default;
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

        /// ポイントを先頭に追加する (Flat 形状向け)
        /// @param tip   刃先ワールド座標
        /// @param root  根本ワールド座標
        void AddPoint(const Vector3& tip, const Vector3& root);

        /// ポイントを先頭に追加する (Arc / Fan 形状向け)
        /// @param tip      刃先ワールド座標
        /// @param root     根本ワールド座標
        /// @param widthDir 幅方向の単位ベクトル (剣の横向きなど)
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

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

        /// 断面形状タイプを設定する
        void SetShapeType(TrailShapeType type) { shapeType_ = type; }
        TrailShapeType GetShapeType() const { return shapeType_; }

        /// 幅方向の分割数を設定する (Flat の場合は無視, Arc/Fan は 2 以上推奨)
        void SetWidthSegments(int segments) { widthSegments_ = std::max(1, segments); }
        int GetWidthSegments() const { return widthSegments_; }

        /// Arc 形状の弧の角度 (度数法, 両側なので 180 = 半円)
        void SetArcAngleDeg(float deg) { arcAngleDeg_ = deg; }
        float GetArcAngleDeg() const { return arcAngleDeg_; }

    private:
        // --------------------------------------------------------
        // 内部処理
        // --------------------------------------------------------

        /// points_ から ProceduralMeshVertex のストリップを生成
        void RebuildVertices();

        /// Flat (平板) 形状のストリップ生成
        void RebuildFlat();

        /// Arc (円弧断面) 形状のストリップ生成
        void RebuildArc();

        /// Fan (扇形断面) 形状のストリップ生成
        void RebuildFan();

        ///
        void RebuildCustom();

        bool IsPointInTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c) const;

        /// age を [0,1] に正規化 (0=新しい端, 1=古い端)
        float NormalizeAge(float age) const
        {
            return (param_.lifetime > 0.f) ? std::min(age / param_.lifetime, 1.f) : 1.f;
        }

        /// フェードカラーを計算
        Vector4 CalcFadeColor(float normalizedAge) const
        {
            const float fade = 1.f - normalizedAge;
            return { 1.f, 1.f, 1.f, fade };
        }

    private:
        TrailEffectParam param_;

        // 断面形状
        TrailShapeType shapeType_ = TrailShapeType::Flat;
        int            widthSegments_ = 1;   // 幅方向の分割数 (Flat=1固定, Arc/Fan は可変)
        float          arcAngleDeg_ = 120.f; // Arc 形状の弧の角度 (度数法)

        // ポイント履歴 (front = 最新, back = 最古)
        std::deque<TrailPoint> points_;

        // 頂点バッファの CPU 側コピー
        std::vector<ProceduralMeshVertex> vertices_;

        // 累積時間 (UV スクロール用)
        float time_ = 0.f;
    };

} // namespace YoRigine
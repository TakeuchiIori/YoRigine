#pragma once
// ===========================================================
// TrailMesh.h
//
// 剣などの軌跡を表現するプロシージャルリボンメッシュ。
// UE Niagara Ribbon を参考に Catmull-Rom スムージングを適用し、
// Flat / Arc / Fan / Primitive の 4 形状に対応する。
//
// 点の追加は AddPoint() で行い、lifetime 秒後に古い点から消える。
// 実際の PSO バインドと CB 書き込みは TrailMeshEmitter が担当する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <deque>

namespace YoRigine {

    /// 軌跡の 1 制御点（tip=刃先側、root=柄側の 2 端点 + 幅方向 + 経過秒）
    struct TrailPoint
    {
        Vector3 tip;       ///< 刃先側ワールド座標
        Vector3 root;      ///< 柄側ワールド座標
        Vector3 widthDir;  ///< 幅方向ベクトル（Arc/Fan の断面計算に使う）
        float   age;       ///< この点が追加されてからの経過秒
    };

    class TrailMesh : public ProceduralMeshBase
    {
    public:
        TrailMesh()  = default;
        ~TrailMesh() override = default;

        // ── 初期化 ───────────────────────────────────────────────

        /// パラメータを渡して頂点バッファを確保する
        void Initialize(const TrailEffectParam& param);

        /// パラメータを差し替える（エディタのホットリロード用）
        void ApplyParam(const TrailEffectParam& param);

        /// 蓄積された制御点を全てクリアする
        void Clear();

        // ── 制御点の追加 ─────────────────────────────────────────

        /// 軌跡の先端（tip）と根本（root）を追加する
        void AddPoint(const Vector3& tip, const Vector3& root);

        /// 幅方向ベクトルを明示して追加する（Arc/Fan 形状で使う）
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

        // ── 毎フレーム ────────────────────────────────────────────

        /// 経過秒を加算して古い点を寿命で消去し頂点を再構築する
        void Update(float deltaTime) override;

        /// 頂点バッファをバインドして描画する（PSO バインドは TrailMeshEmitter が行う）
        void Draw(ID3D12GraphicsCommandList* cmdList) override;

        // ── アクセサ ─────────────────────────────────────────────

        size_t                  GetPointCount() const { return points_.size(); }
        const TrailEffectParam& GetParam()      const { return param_; }

        void           SetShapeType(TrailShapeType type)   { shapeType_ = type; }
        TrailShapeType GetShapeType()                const { return shapeType_; }

        /// 幅方向の分割数（Arc 断面の精度）
        void  SetWidthSegments(int segments) { widthSegments_ = std::max(1, segments); }
        int   GetWidthSegments()       const { return widthSegments_; }

        /// Arc 形状の開き角（度）
        void  SetArcAngleDeg(float deg) { arcAngleDeg_ = deg; }
        float GetArcAngleDeg()    const { return arcAngleDeg_; }

    private:
        // ── 形状別頂点構築 ────────────────────────────────────────

        void RebuildVertices();
        void RebuildFlat();       ///< 平板リボン
        void RebuildArc();        ///< 弧断面リボン
        void RebuildFan();        ///< 扇断面リボン
        void RebuildCustom();     ///< カスタム頂点リスト
        void RebuildPrimitive();  ///< 3D プリミティブ（Box/Sphere/Capsule 等）

        bool IsPointInTriangle(const Vector2& p,
                               const Vector2& a,
                               const Vector2& b,
                               const Vector2& c) const;

        /// 幅スケールを計算する（crescentShape + widthWave を統合）
        /// normalizedAge: 0=新しい端 / 1=古い端
        float CalcWidthScale(float normalizedAge, float tY) const;

        float NormalizeAge(float age) const
        {
            return (param_.lifetime > 0.f)
                ? std::min(age / param_.lifetime, 1.f)
                : 1.f;
        }

        Vector4 CalcFadeColor(float normalizedAge) const
        {
            return { 1.f, 1.f, 1.f, 1.f - normalizedAge };
        }

        static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    private:
        TrailEffectParam param_;

        TrailShapeType shapeType_     = TrailShapeType::Flat;
        int            widthSegments_ = 1;
        float          arcAngleDeg_   = 120.f;

        std::deque<TrailPoint>            points_;   ///< 時系列制御点（古い順）
        std::vector<ProceduralMeshVertex> vertices_;

        float time_ = 0.f;
    };

} // namespace YoRigine

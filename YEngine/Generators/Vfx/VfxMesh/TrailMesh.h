#pragma once
// ===========================================================
// TrailMesh.h
// ★ UE Niagara Ribbon 参考拡張
//   - CalcWidthScale (crescentShape + widthWave 統合)
//   - Arc/Fan でも Catmull-Rom スムージング
// ===========================================================
#include "ProceduralMeshBase.h"
#include "VfxEffectAsset.h"
#include <deque>

namespace YoRigine {

    struct TrailPoint
    {
        Vector3 tip;
        Vector3 root;
        Vector3 widthDir;
        float   age;
    };

    class TrailMesh : public ProceduralMeshBase
    {
    public:
        TrailMesh()  = default;
        ~TrailMesh() override = default;

        // --------------------------------------------------------
        void Initialize(const TrailEffectParam& param);
        void ApplyParam(const TrailEffectParam& param);
        void Clear();

        // --------------------------------------------------------
        void AddPoint(const Vector3& tip, const Vector3& root);
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

        void Update(float deltaTime) override;
        void Draw(ID3D12GraphicsCommandList* cmdList) override;

        // --------------------------------------------------------
        size_t         GetPointCount()  const { return points_.size(); }
        const TrailEffectParam& GetParam() const { return param_; }

        void           SetShapeType(TrailShapeType type)  { shapeType_ = type; }
        TrailShapeType GetShapeType() const { return shapeType_; }

        void  SetWidthSegments(int segments) { widthSegments_ = std::max(1, segments); }
        int   GetWidthSegments() const { return widthSegments_; }

        void  SetArcAngleDeg(float deg) { arcAngleDeg_ = deg; }
        float GetArcAngleDeg() const { return arcAngleDeg_; }

    private:
        void RebuildVertices();
        void RebuildFlat();
        void RebuildArc();
        void RebuildFan();
        void RebuildCustom();
        void RebuildPrimitive();   // ★NEW 3D プリミティブ

        bool IsPointInTriangle(const Vector2& p,
                               const Vector2& a,
                               const Vector2& b,
                               const Vector2& c) const;

        // ★ 統合された幅スケール計算
        // normalizedAge: 0=新しい端, 1=古い端
        // tY           : [0,1] トレイル上の位置
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

        // lerp ヘルパー (MathFunc.h に無い場合の保険)
        static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    private:
        TrailEffectParam param_;

        TrailShapeType shapeType_     = TrailShapeType::Flat;
        int            widthSegments_ = 1;
        float          arcAngleDeg_   = 120.f;

        std::deque<TrailPoint>            points_;
        std::vector<ProceduralMeshVertex> vertices_;

        float time_ = 0.f;
    };

} // namespace YoRigine

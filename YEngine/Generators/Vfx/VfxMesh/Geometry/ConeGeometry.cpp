// ===========================================================
// ConeGeometry.cpp
// ===========================================================
#include "ConeGeometry.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

ConeGeometry::ConeGeometry(const ConeGeomParams& params)
    : radius_(params.radius)
    , height_(params.height)
    , segments_(std::max(3, params.segments))
{}

// ===========================================================
// 頂点生成
// ===========================================================

void ConeGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                          const VfxGeomState& state)
{
    const Vector3&    base = state.position;
    const Quaternion& rot  = state.rotation;
    const float       r    = radius_ * state.scale;
    const float       h    = height_ * state.scale;

    // ローカル軸 Y+ を rotation で回して先端を求める（竜巻の傾き・噴射方向）
    const Vector3 apex = base + rot * Vector3{ 0.f, h, 0.f };

    // ローカル底面（XZ 平面の円）を rotation で回してワールド座標に落とすヘルパー
    auto ringPoint = [&](float ang) -> Vector3 {
        const Vector3 local = { std::cos(ang) * r, 0.f, std::sin(ang) * r };
        return base + rot * local;
    };

    for (int i = 0; i < segments_; ++i) {
        const float a0 = (static_cast<float>(i)     / segments_) * 2.f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segments_) * 2.f * kPi;
        const float u0 = static_cast<float>(i)     / segments_;
        const float u1 = static_cast<float>(i + 1) / segments_;

        // 底面の頂点（回転適用済み）
        const Vector3 b0 = ringPoint(a0);
        const Vector3 b1 = ringPoint(a1);

        auto mk = [](const Vector3& p, float u, float v) {
            ProceduralMeshVertex vtx;
            vtx.position = p;
            vtx.texcoord = { u, v };
            vtx.color    = { 1.f, 1.f, 1.f, 1.f };
            vtx.age      = 0.f;
            return vtx;
        };

        // 側面三角形（底面 v=0、先端 v=1）
        out.push_back(mk(b0,   u0, 0.f));
        out.push_back(mk(b1,   u1, 0.f));
        out.push_back(mk(apex, (u0 + u1) * 0.5f, 1.f));

        // 底面三角形（中心向き）
        const Vector3 bc = base; // 底面中心
        out.push_back(mk(bc, 0.5f, 0.f));
        out.push_back(mk(b1, u1,   0.f));
        out.push_back(mk(b0, u0,   0.f));
    }
}

// ===========================================================
// ダーティ判定
// ===========================================================

bool ConeGeometry::IsDirty(const VfxGeomState& prev,
                             const VfxGeomState& curr) const
{
    return prev.position.x != curr.position.x ||
           prev.position.y != curr.position.y ||
           prev.position.z != curr.position.z ||
           prev.scale      != curr.scale      ||
           prev.rotation.x != curr.rotation.x ||
           prev.rotation.y != curr.rotation.y ||
           prev.rotation.z != curr.rotation.z ||
           prev.rotation.w != curr.rotation.w;
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc ConeGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::Cone;
    d.displayName   = "Cone";
    d.defaultParams = ConeGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<ConeGeometry>(std::get<ConeGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<ConeGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("底面半径##cone", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::DragFloat("高さ##cone", p.height, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderInt("角度分割##cone", p.segments, 3, 64);
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

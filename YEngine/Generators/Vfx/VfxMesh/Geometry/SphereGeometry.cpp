// ===========================================================
// SphereGeometry.cpp
// ===========================================================
#include "SphereGeometry.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

SphereGeometry::SphereGeometry(const SphereGeomParams& params)
    : radius_(params.radius)
    , rings_(std::max(3, params.rings))
    , sectors_(std::max(3, params.sectors))
{}

// ===========================================================
// 頂点生成
// ===========================================================

void SphereGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                            const VfxGeomState& state)
{
    // 球は回転対称なので state.rotation は意図的に無視する
    // （ノイズはワールド座標参照のため頂点回転しても見た目に出ない）
    const Vector3& center = state.position;
    const float    r      = radius_ * state.scale;

    // UV 球をワールド空間で直接生成（VS は World 行列を使わない）
    // texcoord = (経度 u, 緯度 v)。シェーダのノイズ参照座標に使う。
    auto vertexAt = [&](int ring, int sector) -> ProceduralMeshVertex {
        const float v     = static_cast<float>(ring)   / static_cast<float>(rings_);
        const float u     = static_cast<float>(sector) / static_cast<float>(sectors_);
        const float theta = v * kPi;       // 0..π (緯度)
        const float phi   = u * 2.f * kPi; // 0..2π (経度)
        const float sinT  = std::sin(theta);
        const Vector3 dir = {
            sinT * std::cos(phi),
            std::cos(theta),
            sinT * std::sin(phi)
        };
        ProceduralMeshVertex vtx;
        vtx.position = { center.x + dir.x * r,
                         center.y + dir.y * r,
                         center.z + dir.z * r };
        vtx.texcoord = { u, v };
        vtx.color    = { 1.f, 1.f, 1.f, 1.f };
        vtx.age      = 0.f;
        return vtx;
    };

    // TRIANGLELIST（時計回り）
    for (int ri = 0; ri < rings_; ++ri) {
        for (int si = 0; si < sectors_; ++si) {
            ProceduralMeshVertex v00 = vertexAt(ri,     si);
            ProceduralMeshVertex v10 = vertexAt(ri + 1, si);
            ProceduralMeshVertex v01 = vertexAt(ri,     si + 1);
            ProceduralMeshVertex v11 = vertexAt(ri + 1, si + 1);
            out.push_back(v00); out.push_back(v10); out.push_back(v01);
            out.push_back(v10); out.push_back(v11); out.push_back(v01);
        }
    }
}

// ===========================================================
// ダーティ判定
// ===========================================================

bool SphereGeometry::IsDirty(const VfxGeomState& prev,
                               const VfxGeomState& curr) const
{
    return prev.position.x != curr.position.x ||
           prev.position.y != curr.position.y ||
           prev.position.z != curr.position.z ||
           prev.scale      != curr.scale;
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc SphereGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::Sphere;
    d.displayName   = "Sphere";
    d.defaultParams = SphereGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<SphereGeometry>(std::get<SphereGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<SphereGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("半径##sph", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderInt("縦分割(rings)##sph", p.rings, 3, 48);
        c |= YEditorWidget::SliderInt("横分割(sectors)##sph", p.sectors, 3, 64);
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

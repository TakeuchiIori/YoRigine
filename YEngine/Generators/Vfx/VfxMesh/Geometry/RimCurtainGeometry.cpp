// ===========================================================
// RimCurtainGeometry.cpp
// ===========================================================
#include "RimCurtainGeometry.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

RimCurtainGeometry::RimCurtainGeometry(const RimCurtainGeomParams& params)
    : radius_(params.radius)
    , heightRatio_(params.heightRatio)
    , segments_(std::max(3, params.segments))
{}

// ===========================================================
// 頂点生成（円筒の側面。下辺 y=center.y / 上辺 y=center.y+height）
// ===========================================================

void RimCurtainGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                                const VfxGeomState& state)
{
    const Vector3& center = state.position;
    const float    rad    = radius_ * state.scale;
    const float    height = rad * heightRatio_;   // 高さは半径に比例
    const float    yBot   = center.y;
    const float    yTop   = center.y + height;

    // 縦横比 aspect = 円周 / 高さ = (2π·rad) / (rad·heightRatio) = 2π / heightRatio
    // （半径に依存しない）。RimFx シェーダーが等方座標化して「縁が低いと潰れる」のを防ぐ。
    const float aspect = 6.2831853f / std::max(heightRatio_, 0.01f);

    auto mk = [&](float cx, float cz, float y, float u, float v) {
        ProceduralMeshVertex vtx;
        vtx.position = { cx, y, cz };
        vtx.texcoord = { u, v };
        vtx.color    = { 1.f, 1.f, 1.f, 1.f };
        vtx.age      = aspect;   // RimFx が texcoord を等方化するのに使う
        return vtx;
    };

    for (int i = 0; i < segments_; ++i) {
        const float a0 = (static_cast<float>(i)     / segments_) * 2.f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segments_) * 2.f * kPi;
        const float u0 = static_cast<float>(i)     / segments_;
        const float u1 = static_cast<float>(i + 1) / segments_;

        const float x0 = center.x + std::cos(a0) * rad;
        const float z0 = center.z + std::sin(a0) * rad;
        const float x1 = center.x + std::cos(a1) * rad;
        const float z1 = center.z + std::sin(a1) * rad;

        // 下辺(v=0)→上辺(v=1) の四角形を 2 三角形で
        out.push_back(mk(x0, z0, yBot, u0, 0.f));
        out.push_back(mk(x0, z0, yTop, u0, 1.f));
        out.push_back(mk(x1, z1, yBot, u1, 0.f));

        out.push_back(mk(x0, z0, yTop, u0, 1.f));
        out.push_back(mk(x1, z1, yTop, u1, 1.f));
        out.push_back(mk(x1, z1, yBot, u1, 0.f));
    }
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc RimCurtainGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::RimCurtain;
    d.displayName   = "RimCurtain";
    d.defaultParams = RimCurtainGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<RimCurtainGeometry>(std::get<RimCurtainGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<RimCurtainGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("半径##rim", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderFloat("高さ比(半径×)##rim", p.heightRatio, 0.02f, 2.f, "%.2f");
        c |= YEditorWidget::SliderInt("角度分割##rim", p.segments, 3, 128);
        ImGui::TextDisabled("  縁に立つ縦の帯。下→上へ立ち上がる演出用（RimFx と組む）");
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

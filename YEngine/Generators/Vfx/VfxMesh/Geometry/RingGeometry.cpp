// ===========================================================
// RingGeometry.cpp
// ===========================================================
#include "RingGeometry.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

RingGeometry::RingGeometry(const RingGeomParams& params)
    : outerRadius_(params.outerRadius)
    , innerRatio_(std::clamp(params.innerRatio, 0.f, 0.99f))
    , segments_(std::max(3, params.segments))
    , billboard_(params.billboard)
{}

// ===========================================================
// 頂点生成
// ===========================================================

void RingGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                          const VfxGeomState& state)
{
    const Vector3& center  = state.position;
    const float    outer   = outerRadius_ * state.scale;
    const float    inner   = outer * innerRatio_;

    // リング平面の基底ベクトルを決定
    //   billboard : カメラを向く（空中エフェクト）
    //   oriented  : rotation で指定した平面（地面デカールなど）
    Vector3 right = { 1.f, 0.f, 0.f };
    Vector3 up    = { 0.f, 1.f, 0.f };

    if (billboard_ && state.camera) {
        const Vector3 camPos = state.camera->GetTranslate();
        const Vector3 toCam  = Normalize(camPos - center);
        const Vector3 worldUp = (std::fabs(toCam.y) < 0.99f)
            ? Vector3{ 0.f, 1.f, 0.f } : Vector3{ 1.f, 0.f, 0.f };
        right = Normalize(Cross(worldUp, toCam));
        up    = Cross(toCam, right);
    } else {
        // rotation で基底を回す（identity なら XY 平面＝正面向き）
        right = state.rotation * Vector3{ 1.f, 0.f, 0.f };
        up    = state.rotation * Vector3{ 0.f, 1.f, 0.f };
    }

    // アニュラス: セグメントごとに外辺・内辺の 2 四角形を TRIANGLELIST で積む
    for (int i = 0; i < segments_; ++i) {
        const float a0 = (static_cast<float>(i)     / segments_) * 2.f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segments_) * 2.f * kPi;
        const float u0 = static_cast<float>(i)     / segments_;
        const float u1 = static_cast<float>(i + 1) / segments_;

        // 外周点
        const Vector3 o0 = { center.x + (right.x * std::cos(a0) + up.x * std::sin(a0)) * outer,
                              center.y + (right.y * std::cos(a0) + up.y * std::sin(a0)) * outer,
                              center.z + (right.z * std::cos(a0) + up.z * std::sin(a0)) * outer };
        const Vector3 o1 = { center.x + (right.x * std::cos(a1) + up.x * std::sin(a1)) * outer,
                              center.y + (right.y * std::cos(a1) + up.y * std::sin(a1)) * outer,
                              center.z + (right.z * std::cos(a1) + up.z * std::sin(a1)) * outer };
        // 内周点
        const Vector3 i0 = { center.x + (right.x * std::cos(a0) + up.x * std::sin(a0)) * inner,
                              center.y + (right.y * std::cos(a0) + up.y * std::sin(a0)) * inner,
                              center.z + (right.z * std::cos(a0) + up.z * std::sin(a0)) * inner };
        const Vector3 i1 = { center.x + (right.x * std::cos(a1) + up.x * std::sin(a1)) * inner,
                              center.y + (right.y * std::cos(a1) + up.y * std::sin(a1)) * inner,
                              center.z + (right.z * std::cos(a1) + up.z * std::sin(a1)) * inner };

        auto mk = [&](const Vector3& p, float u, float v) {
            ProceduralMeshVertex vtx;
            vtx.position = p;
            vtx.texcoord = { u, v };
            vtx.color    = { 1.f, 1.f, 1.f, 1.f };
            vtx.age      = 0.f;
            return vtx;
        };

        // 外辺(v=0) → 内辺(v=1) の台形を 2 三角形で
        out.push_back(mk(o0, u0, 0.f)); out.push_back(mk(i0, u0, 1.f)); out.push_back(mk(o1, u1, 0.f));
        out.push_back(mk(i0, u0, 1.f)); out.push_back(mk(i1, u1, 1.f)); out.push_back(mk(o1, u1, 0.f));
    }
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc RingGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::Ring;
    d.displayName   = "Ring";
    d.defaultParams = RingGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<RingGeometry>(std::get<RingGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<RingGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("外半径##ring", p.outerRadius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderFloat("内半径比##ring", p.innerRatio, 0.f, 0.99f, "%.2f");
        c |= YEditorWidget::SliderInt("角度分割##ring", p.segments, 3, 96);
        c |= ImGui::Checkbox("カメラを向く(ビルボード)##ring", &p.billboard);
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

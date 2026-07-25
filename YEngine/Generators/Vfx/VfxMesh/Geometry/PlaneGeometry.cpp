// ===========================================================
// PlaneGeometry.cpp
// ===========================================================
#include "PlaneGeometry.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

PlaneGeometry::PlaneGeometry(const PlaneGeomParams& params)
    : size_(params.size), billboard_(params.billboard) {}

// ===========================================================
// 頂点生成
// ===========================================================

void PlaneGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                           const VfxGeomState& state)
{
    const Vector3& center = state.position;
    const float    h      = size_ * state.scale;

    // クワッド平面の基底ベクトルを決定
    //   billboard : カメラを向く
    //   oriented  : rotation で指定した平面（地面デカールなど）
    Vector3 right = { 1.f, 0.f, 0.f };
    Vector3 up    = { 0.f, 1.f, 0.f };

    if (billboard_ && state.camera) {
        const Vector3 camPos  = state.camera->GetTranslate();
        const Vector3 toCam   = Normalize(camPos - center);
        const Vector3 worldUp = (std::fabs(toCam.y) < 0.99f)
            ? Vector3{ 0.f, 1.f, 0.f } : Vector3{ 1.f, 0.f, 0.f };
        right = Normalize(Cross(worldUp, toCam));
        up    = Cross(toCam, right);
    } else {
        // rotation で基底を回す（identity なら XY 平面＝正面向き）
        right = state.rotation * Vector3{ 1.f, 0.f, 0.f };
        up    = state.rotation * Vector3{ 0.f, 1.f, 0.f };
    }

    const Vector3 r = { right.x * h, right.y * h, right.z * h };
    const Vector3 u = { up.x    * h, up.y    * h, up.z    * h };

    auto mk = [&](const Vector3& p, float tu, float tv) {
        ProceduralMeshVertex vtx;
        vtx.position = p;
        vtx.texcoord = { tu, tv };
        vtx.color    = { 1.f, 1.f, 1.f, 1.f };
        vtx.age      = 0.f;
        return vtx;
    };

    // 2 三角形で 1 クワッド（時計回り）
    const Vector3 bl = { center.x - r.x - u.x, center.y - r.y - u.y, center.z - r.z - u.z };
    const Vector3 br = { center.x + r.x - u.x, center.y + r.y - u.y, center.z + r.z - u.z };
    const Vector3 tl = { center.x - r.x + u.x, center.y - r.y + u.y, center.z - r.z + u.z };
    const Vector3 tr = { center.x + r.x + u.x, center.y + r.y + u.y, center.z + r.z + u.z };

    out.push_back(mk(bl, 0.f, 0.f)); out.push_back(mk(tl, 0.f, 1.f)); out.push_back(mk(br, 1.f, 0.f));
    out.push_back(mk(tl, 0.f, 1.f)); out.push_back(mk(tr, 1.f, 1.f)); out.push_back(mk(br, 1.f, 0.f));
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc PlaneGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::Plane;
    d.displayName   = "Plane";
    d.defaultParams = PlaneGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<PlaneGeometry>(std::get<PlaneGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<PlaneGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("サイズ(半辺)##plane", p.size, 0.05f, 0.05f, 50.f, "%.2f");
        c |= ImGui::Checkbox("カメラを向く(ビルボード)##plane", &p.billboard);
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

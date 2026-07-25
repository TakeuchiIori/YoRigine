// ===========================================================
// DiscGeometry.cpp
// ===========================================================
#include "DiscGeometry.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

DiscGeometry::DiscGeometry(const DiscGeomParams& params)
    : radius_(params.radius)
    , segments_(std::max(3, params.segments))
    , billboard_(params.billboard)
    , pitchDeg_(params.pitchDeg)
    , yawDeg_(params.yawDeg)
{}

// ===========================================================
// 頂点生成（中心 + 円周のトライアングルファンを TRIANGLELIST で積む）
// ===========================================================

void DiscGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                          const VfxGeomState& state)
{
    const Vector3& center = state.position;
    const float    rad    = radius_ * state.scale;

    // 円の平面の基底ベクトルを決定。
    //   billboard=true : 完全カメラ向き（毎フレームカメラを追う従来ビルボード）
    //   それ以外       : pitchDeg / yawDeg を三角関数で直接展開（quaternion 非使用・world固定）。
    //                    pitch=0 → XZ 水平（地面デカール） / pitch=90 → 垂直。yaw で傾ける向きを回す。
    Vector3 axisU, axisV;
    if (billboard_ && state.camera) {
        const Vector3 camPos  = state.camera->GetTranslate();
        const Vector3 toCam   = Normalize(camPos - center);
        const Vector3 worldUp = (std::fabs(toCam.y) < 0.99f)
            ? Vector3{ 0.f, 1.f, 0.f } : Vector3{ 1.f, 0.f, 0.f };
        axisU = Normalize(Cross(worldUp, toCam));
        axisV = Cross(toCam, axisU);
    } else {
        const float p  = pitchDeg_ * (kPi / 180.f);
        const float y  = yawDeg_   * (kPi / 180.f);
        const float cp = std::cos(p), sp = std::sin(p);
        const float cy = std::cos(y), sy = std::sin(y);

        // 1) 水平面(XZ)の基底を yaw で回す
        const Vector3 right = { cy, 0.f, -sy }; // Ry(yaw) * (1,0,0)
        const Vector3 fwd   = { sy, 0.f,  cy }; // Ry(yaw) * (0,0,1)

        // 2) world X 軸まわりに pitch だけ起こす（Rx: y' = y cos - z sin, z' = y sin + z cos）
        axisU = { right.x, right.y * cp - right.z * sp, right.y * sp + right.z * cp };
        axisV = { fwd.x,   fwd.y   * cp - fwd.z   * sp, fwd.y   * sp + fwd.z   * cp };
    }

    auto mk = [&](const Vector3& p, float u, float v) {
        ProceduralMeshVertex vtx;
        vtx.position = p;
        vtx.texcoord = { u, v };
        vtx.color    = { 1.f, 1.f, 1.f, 1.f };
        vtx.age      = 0.f;
        return vtx;
    };

    const ProceduralMeshVertex centerVtx = mk(center, 0.5f, 0.5f);

    for (int i = 0; i < segments_; ++i) {
        const float a0 = (static_cast<float>(i)     / segments_) * 2.f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segments_) * 2.f * kPi;

        const float c0 = std::cos(a0), s0 = std::sin(a0);
        const float c1 = std::cos(a1), s1 = std::sin(a1);

        const Vector3 p0 = { center.x + (axisU.x * c0 + axisV.x * s0) * rad,
                              center.y + (axisU.y * c0 + axisV.y * s0) * rad,
                              center.z + (axisU.z * c0 + axisV.z * s0) * rad };
        const Vector3 p1 = { center.x + (axisU.x * c1 + axisV.x * s1) * rad,
                              center.y + (axisU.y * c1 + axisV.y * s1) * rad,
                              center.z + (axisU.z * c1 + axisV.z * s1) * rad };

        // 放射 UV: 中心 0.5 → 縁で length(uv*2-1)=1
        out.push_back(centerVtx);
        out.push_back(mk(p0, 0.5f + 0.5f * c0, 0.5f + 0.5f * s0));
        out.push_back(mk(p1, 0.5f + 0.5f * c1, 0.5f + 0.5f * s1));
    }
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc DiscGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::Disc;
    d.displayName   = "Disc";
    d.defaultParams = DiscGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<DiscGeometry>(std::get<DiscGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<DiscGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("半径##disc", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderInt("角度分割##disc", p.segments, 3, 128);
        c |= ImGui::Checkbox("完全カメラ向き(ビルボード)##disc", &p.billboard);
        if (!p.billboard) {
            c |= YEditorWidget::SliderFloat("起き上がり(pitch)°##disc", p.pitchDeg, 0.f, 90.f, "%.0f");
            c |= YEditorWidget::SliderFloat("向き(yaw)°##disc",       p.yawDeg,  -180.f, 180.f, "%.0f");
            ImGui::TextDisabled("  pitch=0 → 地面に水平 / 90 → 垂直（quaternion 非使用・カメラ非依存）");
        }
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

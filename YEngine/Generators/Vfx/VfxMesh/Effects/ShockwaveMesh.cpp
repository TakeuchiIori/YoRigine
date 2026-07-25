// ===========================================================
// ShockwaveMesh.cpp
// ===========================================================
#include "ShockwaveMesh.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

void ShockwaveMesh::Initialize()
{
    InitBuffer(8);
    vertices_.reserve(8);
}

void ShockwaveMesh::SetTransform(const Vector3& center, float radius)
{
    center_ = center;
    radius_ = radius;
}

void ShockwaveMesh::Update(float deltaTime)
{
    if (!param_.isEnable) return;
    time_ += deltaTime;
    RebuildVertices(); // カメラ向きのため毎フレーム再構築
}

// カメラを向くクワッド（半径 radius_）を生成
void ShockwaveMesh::RebuildVertices()
{
    vertices_.clear();
    if (!camera_) return;

    const Vector3 camPos = camera_->GetTranslate();
    Vector3 toCam = Normalize(camPos - center_);
    Vector3 worldUp = (std::fabs(toCam.y) < 0.99f) ? Vector3{ 0,1,0 } : Vector3{ 1,0,0 };
    Vector3 right = Normalize(Cross(worldUp, toCam));
    Vector3 up    = Cross(toCam, right);

    const float h = radius_;
    Vector3 r = right * h;
    Vector3 u = up * h;

    auto mk = [&](const Vector3& pos, float tu, float tv) {
        ProceduralMeshVertex v;
        v.position = pos;
        v.texcoord = { tu, tv };
        v.color    = param_.color;
        v.age      = 0.0f;
        return v;
    };

    Vector3 c = center_;
    ProceduralMeshVertex bl = mk(c - r - u, 0.0f, 0.0f);
    ProceduralMeshVertex br = mk(c + r - u, 1.0f, 0.0f);
    ProceduralMeshVertex tl = mk(c - r + u, 0.0f, 1.0f);
    ProceduralMeshVertex tr = mk(c + r + u, 1.0f, 1.0f);

    vertices_.push_back(bl); vertices_.push_back(br); vertices_.push_back(tr);
    vertices_.push_back(bl); vertices_.push_back(tr); vertices_.push_back(tl);

    UploadVertices(vertices_);
}

void ShockwaveMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_ || !param_.isEnable) return;
    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

static constexpr size_t kCBAlign256_S = 256;
template<typename T>
static constexpr size_t CBSize256_S() { return (sizeof(T) + kCBAlign256_S - 1) & ~(kCBAlign256_S - 1); }

size_t ShockwaveMesh::GetCBByteSize() const { return CBSize256_S<ShockwaveParamsCB>(); }

void ShockwaveMesh::FillCB(void* mapped, const CBFillArgs& args) const
{
    const auto& sw = args.def.shockwave;
    auto& cb = *static_cast<ShockwaveParamsCB*>(mapped);
    cb.color     = { sw.color.x * args.tint.x, sw.color.y * args.tint.y,
                     sw.color.z * args.tint.z, sw.color.w * args.tint.w };
    cb.thickness  = sw.thickness;
    cb.ringRadius = 0.8f;         // 固定（膨張は radius スケール＝Drive で表現）
    cb._pad0 = cb._pad1 = 0.f;
}

// ===========================================================
// Describe()
// ===========================================================
VfxElementDesc ShockwaveMesh::Describe()
{
    VfxElementDesc d;
    d.type        = VfxElementType::ShockwaveRing;
    d.displayName = "ShockwaveRing";

    d.applyDefaults = [](VfxElement& sub) {
        auto& sw = sub.shockwave;
        sw.radius    = 4.0f;
        sw.duration  = 0.5f;
        sw.thickness = 0.25f;
        sw.color     = { 1.2f, 1.1f, 0.8f, 1.0f };
    };

    d.create = [](const VfxElement& def, const Vector3& pos, float scale, YoRigine::Camera* cam) {
        auto m = std::make_unique<ShockwaveMesh>();
        m->Initialize();
        m->SetCamera(cam);
        m->ApplyParam(def.shockwave);
        m->SetTransform(pos, def.shockwave.radius * scale);
        return m;
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxElement& sub, const VfxEffectAsset& asset, const VfxElementDesc::CommitFn& commit) {
        auto& sw = sub.shockwave;

        YEditorWidget::SectionHeader("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = asset;
            if (YEditorWidget::ColorHDR("色##sw", sw.color))
                commit(b, "ShockwaveRing 色");
        }

        YEditorWidget::SectionHeader("リング / 速度");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::DragFloat("最大半径##swr", sw.radius, 0.05f, 0.1f, 50.0f, "%.2f");
            c |= YEditorWidget::DragFloat("膨張時間(秒)##swd", sw.duration, 0.01f, 0.05f, 5.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("リング太さ##swt", sw.thickness, 0.01f, 1.0f);
            if (c) commit(b, "ShockwaveRing パラメータ");
        }
    };
#endif

    return d;
}

} // namespace YoRigine

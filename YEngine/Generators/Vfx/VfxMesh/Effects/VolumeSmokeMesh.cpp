// ===========================================================
// VolumeSmokeMesh.cpp
// ===========================================================
#include "VolumeSmokeMesh.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

void VolumeSmokeMesh::Initialize(int rings, int sectors)
{
    rings_   = (rings   < 3) ? 3 : rings;
    sectors_ = (sectors < 3) ? 3 : sectors;

    // 頂点数の上限: rings * sectors * 6（TRIANGLELIST）
    const size_t capacity = static_cast<size_t>(rings_) * sectors_ * 6 + 16;
    InitBuffer(capacity);
    vertices_.reserve(capacity);
    dirty_ = true;
}

void VolumeSmokeMesh::SetTransform(const Vector3& center, float radius)
{
    if (center_.x != center.x || center_.y != center.y || center_.z != center.z ||
        radius_ != radius) {
        center_ = center;
        radius_ = radius;
        dirty_  = true;
    }
}

// 共有状態から中心・半径を反映するだけ。膨張(ScaleOverLife)・上昇(Rise) などの
// 動きはすべてモジュールが s.scale / s.position に積んでくれるので、ここではハードコードしない。
void VolumeSmokeMesh::Drive(const VfxEvalState& s)
{
    SetTransform(s.position, param_.radius * s.scale);
}

void VolumeSmokeMesh::Update(float deltaTime)
{
    time_ += deltaTime;
    if (dirty_) {
        RebuildVertices();
        dirty_ = false;
    }
}

// UV 球をワールド空間で生成（VfxMesh.VS は World 行列を使わない）。
// texcoord は (経度 u, 緯度 v)。ノイズの参照座標に使う。
void VolumeSmokeMesh::RebuildVertices()
{
    vertices_.clear();

    auto vertexAt = [&](int ring, int sector) -> ProceduralMeshVertex {
        float v = static_cast<float>(ring)   / static_cast<float>(rings_);   // 0..1 (緯度)
        float u = static_cast<float>(sector) / static_cast<float>(sectors_); // 0..1 (経度)

        float theta = v * kPi;          // 0..π
        float phi   = u * 2.0f * kPi;   // 0..2π

        float sinT = std::sin(theta);
        Vector3 dir = {
            sinT * std::cos(phi),
            std::cos(theta),
            sinT * std::sin(phi)
        };

        ProceduralMeshVertex out;
        out.position = {
            center_.x + dir.x * radius_,
            center_.y + dir.y * radius_,
            center_.z + dir.z * radius_
        };
        out.texcoord = { u, v };
        out.color    = color_;
        out.age      = v; // 縦方向グラデ用（未使用でも可）
        return out;
    };

    // 各クワッドを 2 三角形（TRIANGLELIST）で出力
    for (int ring = 0; ring < rings_; ++ring) {
        for (int sector = 0; sector < sectors_; ++sector) {
            ProceduralMeshVertex v00 = vertexAt(ring,     sector);
            ProceduralMeshVertex v01 = vertexAt(ring,     sector + 1);
            ProceduralMeshVertex v10 = vertexAt(ring + 1, sector);
            ProceduralMeshVertex v11 = vertexAt(ring + 1, sector + 1);

            // 三角形1: v00, v10, v11
            vertices_.push_back(v00);
            vertices_.push_back(v10);
            vertices_.push_back(v11);
            // 三角形2: v00, v11, v01
            vertices_.push_back(v00);
            vertices_.push_back(v11);
            vertices_.push_back(v01);
        }
    }

    UploadVertices(vertices_);
}

void VolumeSmokeMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_) return;

    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    // PSO / RootSig / CBV は呼び出し元でセット済みを前提
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

static constexpr size_t kCBAlign256 = 256;
template<typename T>
static constexpr size_t CBSize256() { return (sizeof(T) + kCBAlign256 - 1) & ~(kCBAlign256 - 1); }

size_t VolumeSmokeMesh::GetCBByteSize() const { return CBSize256<SmokeParamsCB>(); }

void VolumeSmokeMesh::FillCB(void* mapped, const CBFillArgs& args) const
{
    const auto& sm = args.def.smoke;
    auto& cb = *static_cast<SmokeParamsCB*>(mapped);
    auto t = [&](const Vector4& c) -> Vector4 {
        return { c.x * args.tint.x, c.y * args.tint.y, c.z * args.tint.z, c.w * args.tint.w };
    };
    cb.color         = t(sm.color);
    cb.smokeColor    = t(sm.smokeColor);
    cb.center        = center_;
    cb.radius        = radius_;
    cb.time          = args.age;
    cb.noiseScale    = sm.noiseScale;
    cb.noiseStrength = sm.noiseStrength;
    cb.scrollSpeed   = sm.scrollSpeed;
    cb.fresnelPower  = sm.fresnelPower;
    cb.density       = sm.density;
    cb.noiseOctaves  = sm.noiseOctaves;
    cb.rimIntensity  = sm.rimIntensity;
    cb.burst         = -1.f;
    cb._pad2[0] = cb._pad2[1] = cb._pad2[2] = 0.f;
}

// ===========================================================
// Describe()
// ===========================================================
VfxElementDesc VolumeSmokeMesh::Describe()
{
    auto applyDefaults = [](VfxElement& sub) {
        auto& sm = sub.smoke;
        sm.builtInBurstMotion = false;
        sm.riseSpeed = 0.f;
        sm.color = { 1.0f, 1.0f, 1.0f, 1.0f };

        VfxModule grow;
        grow.type = VfxModuleType::ScaleOverLife;
        grow.ease = VfxEase::EaseOutExpo;
        grow.window = 2.0f;
        grow.scaleStart = 0.3f;
        grow.scaleEnd = 1.6f;
        sub.modules.push_back(grow);

        VfxModule rise;
        rise.type = VfxModuleType::Rise;
        rise.startTime = 0.2f;
        rise.window = 1.8f;
        rise.velocity = { 0.f, 1.0f, 0.f };
        rise.amplitude = 1.0f;
        sub.modules.push_back(rise);

        VfxModule fade;
        fade.type = VfxModuleType::FadeInOut;
        fade.window = 2.0f;
        fade.fadeIn = 0.05f;
        fade.fadeOut = 0.7f;
        sub.modules.push_back(fade);

        VfxModule color;
        color.type = VfxModuleType::ColorOverLife;
        color.ease = VfxEase::EaseOutCubic;
        color.window = 0.7f;
        color.colorStart = { 1.2f, 0.6f, 0.3f, 1.0f };
        color.colorEnd = { 0.22f, 0.22f, 0.24f, 1.0f };
        sub.modules.push_back(color);

        VfxModule pulse;
        pulse.type = VfxModuleType::ScalePulse;
        pulse.amplitude = 0.08f;
        pulse.frequency = 1.2f;
        sub.modules.push_back(pulse);

        VfxModule shake;
        shake.type = VfxModuleType::Shake;
        shake.amplitude = 0.06f;
        shake.frequency = 1.5f;
        sub.modules.push_back(shake);
    };

    VfxElementDesc d;
    d.type        = VfxElementType::NoiseVolume;
    d.displayName = "NoiseVolume";
    d.applyDefaults = applyDefaults;

    d.create = [](const VfxElement& def, const Vector3& pos, float scale, Camera*) {
        auto m = std::make_unique<VolumeSmokeMesh>();
        m->Initialize();
        m->ApplyParam(def.smoke);
        m->SetTransform(pos, def.smoke.radius * scale);
        return m;
    };

#ifdef USE_IMGUI
    d.drawUI = [applyDefaults](VfxElement& sub, const VfxEffectAsset& asset, const VfxElementDesc::CommitFn& commit) {
        auto& sm = sub.smoke;

        YEditorWidget::SectionHeader("カラー  (rgb>1 で Bloom / a=濃度)");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::ColorHDR("火球色##sm", sm.color);
            c |= YEditorWidget::ColorHDR("煙色(爆発後)##smk", sm.smokeColor);
            c |= YEditorWidget::DragFloat("上昇速度(爆発後)##smrise", sm.riseSpeed, 0.02f, 0.0f, 8.0f, "%.2f");
            if (c) commit(b, "NoiseVolume 色");
            ImGui::TextDisabled("爆発ワンショット時: 火球色→煙色へ遷移し、上昇しながら漂って消えます");
        }

        YEditorWidget::SectionHeader("ボリューム / 渦巻き");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::DragFloat("半径##smr", sm.radius, 0.05f, 0.1f, 20.0f, "%.2f");
            c |= YEditorWidget::DragFloat("ノイズスケール##sms", sm.noiseScale, 0.05f, 0.1f, 16.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("渦巻きの強さ##smn", sm.noiseStrength, 0.0f, 1.0f);
            c |= YEditorWidget::DragFloat("スクロール速度##smc", sm.scrollSpeed, 0.01f, 0.0f, 3.0f, "%.2f");
            c |= YEditorWidget::DragFloat("縁の柔らかさ##smf", sm.fresnelPower, 0.05f, 0.1f, 8.0f, "%.2f");
            c |= YEditorWidget::DragFloat("密度##smd", sm.density, 0.01f, 0.0f, 3.0f, "%.2f");
            c |= YEditorWidget::DragFloat("オクターブ##smo", sm.noiseOctaves, 0.1f, 1.0f, 5.0f, "%.1f");
            c |= YEditorWidget::DragFloat("リム発光(フレア)##smrim", sm.rimIntensity, 0.05f, 0.0f, 10.0f, "%.2f");
            if (c) commit(b, "NoiseVolume パラメータ");
        }

        YEditorWidget::SectionHeader("動き（モジュールで作る）");
        {
            ImGui::TextDisabled("  NoiseVolume の膨張/上昇/フェードは下の「このエレメントのモジュール」で作ります。");
            if (ImGui::Button("定番の動きをModuleで追加##smtomotion")) {
                VfxEffectAsset b = asset;
                applyDefaults(sub);
                commit(b, "NoiseVolume に定番モジュール追加");
            }
            YEditorWidget::ItemTooltip("膨張/上昇/フェード/色変化/揺れをモジュールとして追加します。");
        }
    };
#endif

    return d;
}

} // namespace YoRigine

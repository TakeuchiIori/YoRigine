// ===========================================================
// LightVolumeMesh.cpp
// ===========================================================
#include "LightVolumeMesh.h"
#include <cmath>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

// 頂点数: 6面 × 2三角形 × 3頂点 = 36
// 余裕を持って 64 で確保
static constexpr size_t kVolumeInitialCapacity = 64;

// -----------------------------------------------------------
void LightVolumeMesh::Initialize(const LightVolumeEffectParam& param)
{
    param_ = param;
    InitBuffer(kVolumeInitialCapacity);
    vertices_.reserve(kVolumeInitialCapacity);
    dirty_ = true;
}

// -----------------------------------------------------------
void LightVolumeMesh::ApplyParam(const LightVolumeEffectParam& param)
{
    param_ = param;
    dirty_ = true;
}

// -----------------------------------------------------------
void LightVolumeMesh::SetTransform(const Vector3& center,
                                   const Vector3& right,
                                   const Vector3& up,
                                   const Vector3& forward)
{
    center_  = center;
    right_   = right;
    up_      = up;
    forward_ = forward;
    dirty_   = true;
}

// -----------------------------------------------------------
void LightVolumeMesh::SetTransform(const Vector3& center, float yawRad)
{
    center_  = center;
    right_   = { std::cos(yawRad), 0.f,  std::sin(yawRad) };
    up_      = { 0.f,              1.f,  0.f              };
    forward_ = { -std::sin(yawRad), 0.f, std::cos(yawRad) };
    dirty_   = true;
}

// -----------------------------------------------------------
void LightVolumeMesh::Update(float deltaTime)
{
    if (!param_.isEnable) return;

    time_ += deltaTime;

    if (dirty_) {
        RebuildVertices();
        dirty_ = false;
    }
    // ※ time_ はシェーダー側の CBV (LightVolumeParams.time) で渡すので
    //   頂点自体は姿勢変化がなければ毎フレーム再構築しない
}

// -----------------------------------------------------------
void LightVolumeMesh::RebuildVertices()
{
    vertices_.clear();

    const Vector3& hx = right_;
    const Vector3& hy = up_;
    const Vector3& hz = forward_;
    const float ex = param_.halfExtents.x;
    const float ey = param_.halfExtents.y;
    const float ez = param_.halfExtents.z;

    // OBB 8 頂点 (ワールド)
    // 命名: P[x符号][y符号][z符号]
    // +x = right 方向, +y = up 方向, +z = forward 方向
    auto make = [&](float sx, float sy, float sz) -> Vector3 {
        return {
            center_.x + hx.x * ex * sx + hy.x * ey * sy + hz.x * ez * sz,
            center_.y + hx.y * ex * sx + hy.y * ey * sy + hz.y * ez * sz,
            center_.z + hx.z * ex * sx + hy.z * ey * sy + hz.z * ez * sz
        };
    };

    const Vector3 Pnnn = make(-1,-1,-1);
    const Vector3 Ppnn = make(+1,-1,-1);
    const Vector3 Pnpn = make(-1,+1,-1);
    const Vector3 Pppn = make(+1,+1,-1);
    const Vector3 Pnnp = make(-1,-1,+1);
    const Vector3 Ppnp = make(+1,-1,+1);
    const Vector3 Pnpp = make(-1,+1,+1);
    const Vector3 Pppp = make(+1,+1,+1);

    const Vector4& col = param_.color;

    // --- 6面を AppendFace で追加 ---
    // age (uvZ) はシェーダーの Z フェードに使う。
    // 1.0 は完全にフェードアウトするため、箱の面自体は内部値として扱う。
    constexpr float kFaceUvZ = 0.5f;

    // Front face (z = +1)
    { Vector3 c[4] = { Pnnp, Ppnp, Pnpp, Pppp }; AppendFace(c, col, kFaceUvZ); }
    // Back face  (z = -1)
    { Vector3 c[4] = { Ppnn, Pnnn, Pppn, Pnpn }; AppendFace(c, col, kFaceUvZ); }
    // Left face  (x = -1)
    { Vector3 c[4] = { Pnnn, Pnnp, Pnpn, Pnpp }; AppendFace(c, col, kFaceUvZ); }
    // Right face (x = +1)
    { Vector3 c[4] = { Ppnp, Ppnn, Pppp, Pppn }; AppendFace(c, col, kFaceUvZ); }
    // Bottom face (y = -1)
    { Vector3 c[4] = { Pnnn, Ppnn, Pnnp, Ppnp }; AppendFace(c, col, kFaceUvZ); }
    // Top face   (y = +1)
    { Vector3 c[4] = { Pnpn, Pnpp, Pppn, Pppp }; AppendFace(c, col, kFaceUvZ); }

    UploadVertices(vertices_);
}

// -----------------------------------------------------------
// 1面 (TriangleList 向け 6 頂点) を追加
//   corners[0]=左下, [1]=右下, [2]=左上, [3]=右上
// -----------------------------------------------------------
void LightVolumeMesh::AppendFace(const Vector3 corners[4],
                                  const Vector4& color,
                                  float          uvZ)
{
    // 4隅: 左下, 右下, 左上, 右上
    const Vector2 uvs[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
    const int indices[6] = { 0, 1, 2, 2, 1, 3 };

    for (int idx : indices)
    {
        ProceduralMeshVertex v;
        v.position = corners[idx];
        v.texcoord = uvs[idx];
        v.color    = color;
        v.age      = uvZ;  // シェーダー内で fadeZ として使用
        vertices_.push_back(v);
    }
}

// -----------------------------------------------------------
void LightVolumeMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_ || !param_.isEnable) return;

    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    // PSO / CBV は呼び出し元でセット済みを前提

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

static constexpr size_t kCBAlign256_V = 256;
template<typename T>
static constexpr size_t CBSize256_V() { return (sizeof(T) + kCBAlign256_V - 1) & ~(kCBAlign256_V - 1); }

size_t LightVolumeMesh::GetCBByteSize() const { return CBSize256_V<LightVolumeParamsCB>(); }

void LightVolumeMesh::FillCB(void* mapped, const CBFillArgs& args) const
{
    const auto& lv = args.def.lightVolume;
    auto& cb = *static_cast<LightVolumeParamsCB*>(mapped);
    cb.color[0] = lv.color.x * args.tint.x;
    cb.color[1] = lv.color.y * args.tint.y;
    cb.color[2] = lv.color.z * args.tint.z;
    cb.color[3] = lv.color.w * lv.intensity * args.tint.w;
    cb.edgeFade      = lv.edgeFade;
    cb.depthFade     = lv.depthFade;
    cb.noiseTiling   = lv.noiseTiling;
    cb.noiseStrength = lv.noiseStrength;
    cb.time          = args.age;
    cb.beamStrength  = lv.beamStrength;
    cb.beamRadius    = lv.beamRadius * args.beamRadiusScale;
    cb.beamPower     = lv.beamPower;
    cb.beamGlow      = lv.beamGlow * args.beamGlowScale;
}

// ===========================================================
// Describe()
// ===========================================================
VfxElementDesc LightVolumeMesh::Describe()
{
    auto applyDefaults = [](VfxElement& sub) {
        auto& lv = sub.lightVolume;
        lv.beamStrength = 0.85f;
        lv.beamRadius   = 0.32f;
        lv.beamPower    = 2.5f;
        lv.beamGlow     = 3.0f;
        lv.edgeFade     = 0.08f;
        lv.depthFade    = 50.0f;

        VfxModule beamPulse;
        beamPulse.type      = VfxModuleType::BeamPulse;
        beamPulse.target    = VfxModuleTarget::LightVolume;
        beamPulse.amplitude = 0.18f;
        beamPulse.frequency = 1.4f;
        sub.modules.push_back(beamPulse);

        VfxModule flicker;
        flicker.type      = VfxModuleType::Flicker;
        flicker.target    = VfxModuleTarget::LightVolume;
        flicker.amplitude = 0.12f;
        flicker.frequency = 9.0f;
        sub.modules.push_back(flicker);
    };

    VfxElementDesc d;
    d.type        = VfxElementType::LightVolume;
    d.displayName = "LightVolume";
    d.applyDefaults = applyDefaults;

    d.create = [](const VfxElement& def, const Vector3& pos, float /*scale*/, YoRigine::Camera*) {
        auto m = std::make_unique<LightVolumeMesh>();
        m->Initialize(def.lightVolume);
        m->SetTransform(pos, 0.f);
        return m;
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxElement& sub, const VfxEffectAsset& asset, const VfxElementDesc::CommitFn& commit) {
        auto& lv = sub.lightVolume;

        YEditorWidget::SectionHeader("OBB サイズ  (X=右 / Y=上 / Z=前)");
        {
            VfxEffectAsset b = asset;
            if (YEditorWidget::DragVec3("半辺長##he", lv.halfExtents, 0.05f, 0.01f, 50.f, "%.2f"))
                commit(b, "Volume サイズ");
        }

        YEditorWidget::SectionHeader("カラー  (α = ボリューム濃度)");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::Color("ボリュームカラー##vc", lv.color);
            c |= YEditorWidget::SliderFloat("輝度##inten", lv.intensity, 0.f, 10.f, "%.2f");
            if (c) commit(b, "Volume カラー");
        }

        YEditorWidget::SectionHeader("フェード / ノイズ");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("エッジフェード##lvedge", lv.edgeFade, 0.001f, 0.5f, "%.3f");
            c |= YEditorWidget::DragFloat("近接フェード距離##lvdepth", lv.depthFade, 0.05f, 0.001f, 50.0f, "%.2f");
            c |= YEditorWidget::DragFloat("ノイズ細かさ##lvnoiseTile", lv.noiseTiling, 0.05f, 0.01f, 50.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("ノイズ強度##lvnoiseStr", lv.noiseStrength, 0.0f, 1.0f, "%.2f");
            if (c) commit(b, "Volume フェード/ノイズ");
        }

        YEditorWidget::SectionHeader("ビーム");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("ビーム強度##lvbeamStr", lv.beamStrength, 0.0f, 1.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("ビーム半径##lvbeamRadius", lv.beamRadius, 0.01f, 1.5f, "%.2f");
            c |= YEditorWidget::DragFloat("ビーム鋭さ##lvbeamPower", lv.beamPower, 0.05f, 0.1f, 12.0f, "%.2f");
            c |= YEditorWidget::DragFloat("ビーム発光##lvbeamGlow", lv.beamGlow, 0.05f, 0.0f, 10.0f, "%.2f");
            if (c) commit(b, "Volume ビーム");
        }
    };
#endif

    return d;
}

} // namespace YoRigine

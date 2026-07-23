// ===========================================================
// LobeClusterGeometry.cpp
// ===========================================================
#include "LobeClusterGeometry.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;
// 黄金角。フィボナッチ球で塊を均等にばらまくのに使う。
static constexpr float kGoldenAngle = 2.399963229728653f;

// ── 決定論的ハッシュ乱数 [0,1) ────────────────────────────
// seed とインデックス(salt)から安定した乱数を作る。std::random より軽く、
// 毎フレーム同じ入力なら同じ配置になる（配置のチラつき防止）。
static float Hash01(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000);
}
static float Rand(uint32_t seed, uint32_t index, uint32_t salt)
{
    return Hash01(seed * 747796405u + index * 2891336453u + salt * 2654435761u);
}

// smoothstep(0,1,t)
static float SmoothStep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

LobeClusterGeometry::LobeClusterGeometry(const LobeClusterGeomParams& params)
{
    ApplyParams(params);
}

void LobeClusterGeometry::ApplyParams(const VfxGeometryParams& p)
{
    const auto& s = std::get<LobeClusterGeomParams>(p);
    radius_     = s.radius;
    lobeCount_  = std::clamp(s.lobeCount, 1, 64);
    lobeRadius_ = std::max(0.01f, s.lobeRadius);
    lobeJitter_ = std::clamp(s.lobeJitter, 0.0f, 1.0f);
    stagger_    = std::clamp(s.stagger, 0.0f, 1.0f);
    rings_      = std::max(3, s.rings);
    sectors_    = std::max(3, s.sectors);
    seed_       = s.seed;
}

// ===========================================================
// 頂点生成
// ===========================================================

void LobeClusterGeometry::Build(std::vector<ProceduralMeshVertex>& out,
                                const VfxGeomState& state)
{
    const Vector3& center      = state.position;
    const float    clusterR    = radius_ * state.scale;
    // progress<0（ループ）は「膨張しきった状態」で常時表示する
    const float    progress    = (state.progress < 0.0f) ? 1.0f : state.progress;
    const float    popWindow   = 0.35f; // 各塊が膨らみきるのにかける progress 幅

    // 1塊ぶんの小球を lobeCenter/lobeR で out に追加する
    auto buildLobe = [&](const Vector3& lobeCenter, float lobeR) {
        auto vertexAt = [&](int ring, int sector) -> ProceduralMeshVertex {
            const float v     = static_cast<float>(ring)   / static_cast<float>(rings_);
            const float u     = static_cast<float>(sector) / static_cast<float>(sectors_);
            const float theta = v * kPi;
            const float phi   = u * 2.f * kPi;
            const float sinT  = std::sin(theta);
            const Vector3 dir = { sinT * std::cos(phi), std::cos(theta), sinT * std::sin(phi) };
            ProceduralMeshVertex vtx;
            vtx.position = { lobeCenter.x + dir.x * lobeR,
                             lobeCenter.y + dir.y * lobeR,
                             lobeCenter.z + dir.z * lobeR };
            // texcoord はワールド由来のノイズ参照に使われるので、塊ごとにズレる u,v で良い
            vtx.texcoord = { u, v };
            vtx.color    = { 1.f, 1.f, 1.f, 1.f };
            vtx.age      = 0.f;
            return vtx;
        };
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
    };

    for (int i = 0; i < lobeCount_; ++i) {
        const uint32_t idx = static_cast<uint32_t>(i);

        // ── 配置：フィボナッチ球で均等に散らし、距離と方向にジッタを足す ──
        const float t   = (static_cast<float>(i) + 0.5f) / static_cast<float>(lobeCount_);
        const float y   = 1.0f - 2.0f * t;               // -1..1
        const float rxz = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float phi = static_cast<float>(i) * kGoldenAngle;
        Vector3 dir = { rxz * std::cos(phi), y, rxz * std::sin(phi) };

        // 方向ジッタ（少し散らす）
        dir.x += (Rand(seed_, idx, 11u) - 0.5f) * 0.4f * lobeJitter_;
        dir.y += (Rand(seed_, idx, 12u) - 0.5f) * 0.4f * lobeJitter_;
        dir.z += (Rand(seed_, idx, 13u) - 0.5f) * 0.4f * lobeJitter_;
        const float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (dl > 1e-5f) { dir.x /= dl; dir.y /= dl; dir.z /= dl; }

        // 中心からの距離：外殻寄り(0.6..1.0)に置くとシルエットがボコボコになる
        const float distFrac = 0.6f + 0.4f * Rand(seed_, idx, 21u);
        const Vector3 lobeCenter = {
            center.x + dir.x * clusterR * distFrac,
            center.y + dir.y * clusterR * distFrac,
            center.z + dir.z * clusterR * distFrac,
        };

        // 各塊の基準半径（大きさジッタ付き）
        float lobeR = clusterR * lobeRadius_;
        lobeR *= (1.0f - lobeJitter_ * 0.6f) + Rand(seed_, idx, 31u) * (lobeJitter_ * 1.2f);

        // ── 時間差ポップ：塊ごとに開始時刻をずらして順番に膨張させる ──
        // popStart は [0, stagger) にインデックス順＋軽いジッタで散らす
        const float order    = (lobeCount_ > 1) ? (static_cast<float>(i) / static_cast<float>(lobeCount_ - 1)) : 0.0f;
        const float popStart = stagger_ * (order * 0.85f + Rand(seed_, idx, 41u) * 0.15f);
        const float local    = (popWindow > 1e-4f) ? (progress - popStart) / popWindow : 1.0f;
        const float growth   = SmoothStep01(local);
        if (growth <= 0.001f) {
            continue; // まだ湧いていない塊はスキップ
        }
        lobeR *= growth;

        buildLobe(lobeCenter, lobeR);
    }
}

// ===========================================================
// ダーティ判定
// ===========================================================

bool LobeClusterGeometry::IsDirty(const VfxGeomState& prev,
                                  const VfxGeomState& curr) const
{
    return prev.position.x != curr.position.x ||
           prev.position.y != curr.position.y ||
           prev.position.z != curr.position.z ||
           prev.scale      != curr.scale      ||
           prev.progress   != curr.progress;   // ポップ膨張の進行
}

// ===========================================================
// GeometryRegistry 登録情報
// ===========================================================

VfxGeometryDesc LobeClusterGeometry::Describe()
{
    VfxGeometryDesc d;
    d.type          = VfxGeometryType::LobeCluster;
    d.displayName   = "LobeCluster (もくもく)";
    d.defaultParams = LobeClusterGeomParams{};

    d.create = [](const VfxGeometryParams& p) -> std::unique_ptr<VfxGeometry> {
        return std::make_unique<LobeClusterGeometry>(std::get<LobeClusterGeomParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxGeometryParams& params) -> bool {
        auto& p = std::get<LobeClusterGeomParams>(params);
        bool c = false;
        c |= YEditorWidget::DragFloat("全体半径##lobe", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::SliderInt("塊の数(もくもく)##lobe", p.lobeCount, 1, 64);
        c |= YEditorWidget::DragFloat("塊の半径比##lobe", p.lobeRadius, 0.01f, 0.05f, 2.0f, "%.2f");
        c |= YEditorWidget::DragFloat("ばらつき##lobe", p.lobeJitter, 0.01f, 0.0f, 1.0f, "%.2f");
        c |= YEditorWidget::DragFloat("時間差ポップ##lobe", p.stagger, 0.01f, 0.0f, 1.0f, "%.2f");
        c |= YEditorWidget::SliderInt("塊の縦分割##lobe", p.rings, 3, 24);
        c |= YEditorWidget::SliderInt("塊の横分割##lobe", p.sectors, 3, 32);
        int seed = static_cast<int>(p.seed);
        if (YEditorWidget::DragInt("シード##lobe", seed, 1.0f, 0, 100000)) {
            p.seed = static_cast<uint32_t>(std::max(0, seed));
            c = true;
        }
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

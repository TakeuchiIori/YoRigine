// ===========================================================
// LightningMesh.cpp
// ===========================================================
#include "LightningMesh.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

// ===========================================================
// ユーティリティ
// ===========================================================

// seed から 0..1 の決定的乱数（Wang hash 系）
static float Hash01(uint32_t n)
{
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return (n & 0x7fffffffu) / static_cast<float>(0x7fffffff);
}

// ===========================================================
// 初期化
// ===========================================================

void LightningMesh::Initialize()
{
    // 本線 + 枝のリボン頂点を想定して多めに確保
    InitBuffer(4096);
    vertices_.reserve(4096);
}

// ===========================================================
// 設定
// ===========================================================

void LightningMesh::SetEndpoints(const Vector3& start, const Vector3& end)
{
    // 端点が変わった時だけ RebuildPaths を走らせる
    if (start_.x != start.x || start_.y != start.y || start_.z != start.z ||
        end_.x   != end.x   || end_.y   != end.y   || end_.z   != end.z)
    {
        endpointsDirty_ = true;
    }
    start_ = start;
    end_   = end;
}

// ===========================================================
// 毎フレーム更新
// ===========================================================

void LightningMesh::Update(float deltaTime)
{
    if (!param_.isEnable) return;
    time_ += deltaTime;

    // flickerRate 回/秒でシードを進める（形が変わるタイミング）
    flickerTimer_ += deltaTime;
    const float interval = (param_.flickerRate > 0.01f) ? (1.0f / param_.flickerRate) : 1.0f;
    while (flickerTimer_ >= interval) {
        flickerTimer_ -= interval;
        flickerSeed_ += 9173u;
    }

    // 経路は flickerSeed_ か端点が変わった時だけ再生成（重い Subdivide をスキップ）
    if (flickerSeed_ != lastBuiltSeed_ || endpointsDirty_) {
        RebuildPaths();
        lastBuiltSeed_  = flickerSeed_;
        endpointsDirty_ = false;
    }

    // リボン展開はカメラ向きが変わるため毎フレーム実行（軽量）
    RebuildRibbons();
}

// ===========================================================
// 経路生成（フリッカー時のみ）
// ===========================================================

// a→b を midpoint displacement で再帰的に折る
// depth 0 で終端点 b を out に追加し、再帰で中間点を挿入する
void LightningMesh::Subdivide(const Vector3& a, const Vector3& b, float amp, int depth, std::vector<Vector3>& out)
{
    if (depth <= 0) {
        out.push_back(b);
        return;
    }

    // a→b 軸に直交する平面上でランダム方向に中点をずらす
    Vector3 axis = Normalize(b - a);
    Vector3 ref = (std::fabs(axis.y) < 0.99f) ? Vector3{ 0,1,0 } : Vector3{ 1,0,0 };
    Vector3 u = Normalize(Cross(axis, ref));
    Vector3 v = Cross(axis, u);

    seed_ += 2654435761u;
    float a0 = Hash01(seed_) * 2.0f * kPi;
    float r0 = (0.4f + 0.6f * Hash01(seed_ * 7u + 3u)) * amp;
    Vector3 disp = (u * std::cos(a0) + v * std::sin(a0)) * r0;

    Vector3 mid = (a + b) * 0.5f + disp;

    Subdivide(a, mid, amp * 0.5f, depth - 1, out);
    Subdivide(mid, b, amp * 0.5f, depth - 1, out);
}

void LightningMesh::RebuildPaths()
{
    cachedPaths_.clear();
    seed_ = flickerSeed_;

    // segments から再帰レベルを決定（2^levels >= segments、上限 7）
    int levels = 1;
    while ((1 << levels) < param_.segments && levels < 7) ++levels;

    // --- 本線 ---
    BoltPath mainPath;
    mainPath.points.push_back(start_);
    mainPath.color = { 0.f, 0.f, 0.f, param_.color.w }; // r=0 → シェーダで本線扱い
    mainPath.width = param_.width;

    if (std::fabs(param_.bendAmount) > 1e-4f) {
        // 弧: 軸垂直方向に bendAmount ずらした制御点を経由して弓なりにする
        Vector3 axis = Normalize(end_ - start_);
        Vector3 ref  = (std::fabs(axis.y) < 0.99f) ? Vector3{ 0,1,0 } : Vector3{ 1,0,0 };
        Vector3 perp = Normalize(Cross(axis, ref));
        Vector3 ctrl = (start_ + end_) * 0.5f + perp * param_.bendAmount;
        int lv = (levels > 1) ? levels - 1 : 1;
        Subdivide(start_, ctrl, param_.jitter, lv, mainPath.points);
        Subdivide(ctrl,   end_, param_.jitter, lv, mainPath.points);
    } else {
        Subdivide(start_, end_, param_.jitter, levels, mainPath.points);
    }

    cachedPaths_.push_back(std::move(mainPath));
    // std::move 後は mainPath が無効なので vector から参照を取り直す
    const auto& mainPts = cachedPaths_[0].points;

    // --- 枝 ---
    for (int bIdx = 0; bIdx < param_.branches; ++bIdx) {
        seed_ += 40503u;
        const int mainSize = static_cast<int>(mainPts.size());
        if (mainSize < 2) break; // 本線の点が足りない場合は枝を出さない

        // 本線上のランダムな点から分岐させる
        int idx = 1 + static_cast<int>(Hash01(seed_) * static_cast<float>(mainSize - 2));
        idx = std::max(1, std::min(idx, mainSize - 1));
        Vector3 origin = mainPts[idx];

        // 本線方向から少しずらした斜め方向へ枝を伸ばす
        Vector3 dir = Normalize(end_ - start_);
        Vector3 ref = (std::fabs(dir.y) < 0.99f) ? Vector3{ 0,1,0 } : Vector3{ 1,0,0 };
        Vector3 u = Normalize(Cross(dir, ref));
        Vector3 v = Cross(dir, u);
        float ang  = Hash01(seed_ * 3u + 1u) * 2.0f * kPi;
        Vector3 off = (u * std::cos(ang) + v * std::sin(ang));
        float blen  = (0.3f + 0.4f * Hash01(seed_ * 11u)) * Length(end_ - start_);
        Vector3 bend = origin + (dir * 0.4f + off * 0.9f) * blen;

        BoltPath branchPath;
        branchPath.points.push_back(origin);
        branchPath.color = { 1.f, 0.f, 0.f, param_.color.w }; // r=1 → シェーダで枝扱い
        branchPath.width = param_.width * 0.6f;
        Subdivide(origin, bend, param_.branchJitter, levels - 1, branchPath.points);
        cachedPaths_.push_back(std::move(branchPath));
    }
}

// ===========================================================
// リボン展開（毎フレーム）
// ===========================================================

// キャッシュ済みの経路点列をカメラ向きのリボン三角形に変換して頂点バッファへ積む
void LightningMesh::AppendBoltRibbon(const std::vector<Vector3>& pts, float width, const Vector4& color)
{
    if (pts.size() < 2 || !camera_) return;

    const Vector3 camPos = camera_->GetTranslate();
    const float half = width * 0.5f;
    const int n = static_cast<int>(pts.size());

    // 各点でセグメント方向 × カメラ方向の外積を取り、左右オフセットを求める
    std::vector<Vector3> left(n), right(n);
    for (int i = 0; i < n; ++i) {
        Vector3 prev   = pts[(i > 0) ? i - 1 : i];
        Vector3 next   = pts[(i < n - 1) ? i + 1 : i];
        Vector3 segDir = Normalize(next - prev);
        Vector3 toCam  = Normalize(camPos - pts[i]);
        Vector3 side   = Cross(segDir, toCam);
        float len = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
        if (len < 1e-4f) side = { 1,0,0 }; else side = side * (1.0f / len);
        left[i]  = pts[i] + side * half;
        right[i] = pts[i] - side * half;
    }

    // セグメントごとに 2 三角形（TRIANGLELIST）として積む
    for (int i = 0; i < n - 1; ++i) {
        float t0 = static_cast<float>(i)     / static_cast<float>(n - 1);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(n - 1);

        auto mk = [&](const Vector3& p, float u, float vCoord) {
            ProceduralMeshVertex out;
            out.position = p;
            out.texcoord = { u, vCoord };
            out.color    = color;
            out.age      = u; // age(0..1) をシェーダのフェードに使う
            return out;
        };

        ProceduralMeshVertex L0 = mk(left[i],      t0, 0.0f);
        ProceduralMeshVertex R0 = mk(right[i],     t0, 1.0f);
        ProceduralMeshVertex L1 = mk(left[i + 1],  t1, 0.0f);
        ProceduralMeshVertex R1 = mk(right[i + 1], t1, 1.0f);

        vertices_.push_back(L0); vertices_.push_back(R0); vertices_.push_back(R1);
        vertices_.push_back(L0); vertices_.push_back(R1); vertices_.push_back(L1);
    }
}

void LightningMesh::RebuildRibbons()
{
    vertices_.clear();
    if (!camera_ || cachedPaths_.empty()) return;

    for (const auto& path : cachedPaths_) {
        AppendBoltRibbon(path.points, path.width, path.color);
    }

    UploadVertices(vertices_);
}

// ===========================================================
// 描画
// ===========================================================

void LightningMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_ || !param_.isEnable) return;
    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

// ===========================================================
// VfxEvalState 反映
// ===========================================================

void LightningMesh::Drive(const VfxEvalState& state)
{
    if (state.useAutoEndpoints) {
        // direction / length から端点を自動計算（エディタプレビュー用）
        Vector3 dir = param_.direction;
        const float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        dir = (dl < 1e-4f) ? Vector3{ 0.f, 1.f, 0.f } : dir * (1.f / dl);
        const float half = param_.length * 0.5f;
        SetEndpoints(state.position - dir * half, state.position + dir * half);
    } else {
        SetEndpoints(state.boltStart, state.boltEnd);
    }
}

// ===========================================================
// 定数バッファ
// ===========================================================

static constexpr size_t kCBAlign256_L = 256;
template<typename T>
static constexpr size_t CBSize256_L() { return (sizeof(T) + kCBAlign256_L - 1) & ~(kCBAlign256_L - 1); }

size_t LightningMesh::GetCBByteSize() const { return CBSize256_L<LightningParamsCB>(); }

void LightningMesh::FillCB(void* mapped, const CBFillArgs& args) const
{
    const auto& lt = args.def.lightning;
    auto& cb = *static_cast<LightningParamsCB*>(mapped);

    // モジュールの色乗算（tint）を各カラーに掛ける
    auto t = [&](const Vector4& c) -> Vector4 {
        return { c.x * args.tint.x, c.y * args.tint.y, c.z * args.tint.z, c.w * args.tint.w };
    };
    cb.color            = t(lt.color);
    cb.glowColor        = t(lt.glowColor);
    cb.branchColor      = t(lt.branchColor);
    cb.time             = args.age;
    cb.glowPower        = lt.glowPower;
    cb.coreWidth        = lt.coreWidth;
    cb.solidness        = lt.solidness;
    cb.outlineIntensity = lt.outlineIntensity;
    cb._pad0 = cb._pad1 = cb._pad2 = 0.f;
}

// ===========================================================
// VfxMeshRegistry 登録情報
// ===========================================================

VfxElementDesc LightningMesh::Describe()
{
    VfxElementDesc d;
    d.type        = VfxElementType::LightningBolt;
    d.displayName = "LightningBolt";

    d.applyDefaults = [](VfxElement& sub) {
        auto& lt = sub.lightning;
        lt.length           = 4.0f;
        lt.width            = 0.08f;
        lt.jitter           = 0.8f;
        lt.segments         = 32;
        lt.branches         = 2;
        lt.branchJitter     = 0.5f;
        lt.flickerRate      = 12.0f;
        lt.glowPower        = 2.0f;
        lt.coreWidth        = 0.3f;
        lt.solidness        = 0.5f;
        lt.outlineIntensity = 1.0f;
        lt.color            = { 0.6f, 0.8f, 1.4f, 1.0f };
        lt.glowColor        = { 0.3f, 0.5f, 1.2f, 0.8f };
        lt.branchColor      = { 0.4f, 0.6f, 1.0f, 0.6f };
        lt.direction        = { 0.f, 1.f, 0.f };
    };

    d.create = [](const VfxElement& def, const Vector3& /*pos*/, float /*scale*/, YoRigine::Camera* cam) {
        auto m = std::make_unique<LightningMesh>();
        m->Initialize();
        m->SetCamera(cam);
        m->ApplyParam(def.lightning);
        return m;
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxElement& sub, const VfxEffectAsset& asset, const VfxElementDesc::CommitFn& commit) {
        auto& lt = sub.lightning;

        YEditorWidget::SectionHeader("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::ColorHDR("芯の色##lt", lt.color);
            c |= YEditorWidget::ColorHDR("グロー色##ltg", lt.glowColor);
            c |= YEditorWidget::ColorHDR("枝の色##ltb", lt.branchColor);
            if (c) commit(b, "LightningBolt 色");
        }

        YEditorWidget::SectionHeader("実体感 / アウトライン");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("芯の太さ##ltcw", lt.coreWidth, 0.0f, 1.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("実体感(透明感↓)##lts", lt.solidness, 0.0f, 1.0f, "%.2f");
            c |= YEditorWidget::SliderFloat("アウトライン強調##lto", lt.outlineIntensity, 0.0f, 4.0f, "%.2f");
            if (c) commit(b, "LightningBolt 実体感");
        }

        YEditorWidget::SectionHeader("方向 / 曲線");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            if (ImGui::SmallButton("縦##ltd")) { lt.direction = { 0,1,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("横##ltd")) { lt.direction = { 1,0,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("奥##ltd")) { lt.direction = { 0,0,1 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("斜め##ltd")) { lt.direction = { 1,1,0 }; c = true; }
            c |= YEditorWidget::DirectionVec3("方向(自由)##ltd", lt.direction, 0.02f);
            c |= YEditorWidget::SliderFloat("曲げ量(弧)##ltbend", lt.bendAmount, -5.0f, 5.0f, "%.2f");
            if (c) commit(b, "LightningBolt 方向/曲線");
        }

        YEditorWidget::SectionHeader("稲妻 / 明滅");
        {
            VfxEffectAsset b = asset;
            bool c = false;
            c |= YEditorWidget::DragFloat("長さ##ltlen", lt.length, 0.05f, 0.1f, 50.0f, "%.2f");
            c |= YEditorWidget::DragFloat("幅##ltw", lt.width, 0.005f, 0.01f, 2.0f, "%.3f");
            c |= YEditorWidget::DragFloat("ジグザグ振れ##ltj", lt.jitter, 0.02f, 0.0f, 5.0f, "%.2f");
            c |= YEditorWidget::SliderInt("分割数##ltsd", lt.segments, 4, 64);
            c |= YEditorWidget::SliderInt("枝の数##ltbn", lt.branches, 0, 8);
            c |= YEditorWidget::DragFloat("枝の振れ##ltbj", lt.branchJitter, 0.02f, 0.0f, 5.0f, "%.2f");
            c |= YEditorWidget::DragFloat("明滅レート(回/秒)##ltf", lt.flickerRate, 0.5f, 0.0f, 60.0f, "%.1f");
            c |= YEditorWidget::DragFloat("芯のグロー##ltgp", lt.glowPower, 0.05f, 0.1f, 8.0f, "%.2f");
            if (c) commit(b, "LightningBolt パラメータ");
        }
    };
#endif

    return d;
}

} // namespace YoRigine

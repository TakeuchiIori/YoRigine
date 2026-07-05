// ===========================================================
// LightningMesh.cpp
// ===========================================================
#include "LightningMesh.h"
#include "Systems/Camera/Camera.h"
#include <cmath>

namespace YoRigine {

static constexpr float kPi = 3.14159265358979323846f;

// seed から 0..1 の決定的乱数
static float Hash01(uint32_t n)
{
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return (n & 0x7fffffffu) / static_cast<float>(0x7fffffff);
}

void LightningMesh::Initialize()
{
    // 本線 + 枝 + リボン頂点。多めに確保。
    InitBuffer(4096);
    vertices_.reserve(4096);
}

void LightningMesh::SetEndpoints(const Vector3& start, const Vector3& end)
{
    start_ = start;
    end_   = end;
}

void LightningMesh::Update(float deltaTime)
{
    if (!param_.isEnable) return;
    time_ += deltaTime;

    // flickerRate 回/秒で形を変える（パチパチ感）
    flickerTimer_ += deltaTime;
    float interval = (param_.flickerRate > 0.01f) ? (1.0f / param_.flickerRate) : 1.0f;
    bool reshaped = false;
    while (flickerTimer_ >= interval) {
        flickerTimer_ -= interval;
        flickerSeed_ += 9173u;   // 形が変わるのはフリッカーの瞬間だけ
        reshaped = true;
    }

    // 形が変わった時 or 毎フレーム（カメラ向きリボンのため）再構築
    RebuildVertices();
    (void)reshaped;
}

// a→b を中点変位で折る。点は a を含めず、中点と b 手前までを out に積む。
void LightningMesh::Subdivide(const Vector3& a, const Vector3& b, float amp, int depth, std::vector<Vector3>& out)
{
    if (depth <= 0) {
        out.push_back(b);
        return;
    }

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

void LightningMesh::AppendBoltRibbon(const std::vector<Vector3>& pts, float width, const Vector4& color)
{
    if (pts.size() < 2 || !camera_) return;

    const Vector3 camPos = camera_->GetTranslate();
    const float half = width * 0.5f;
    const int n = static_cast<int>(pts.size());

    // 各点のリボン左右を求める
    std::vector<Vector3> left(n), right(n);
    for (int i = 0; i < n; ++i) {
        Vector3 prev = pts[(i > 0) ? i - 1 : i];
        Vector3 next = pts[(i < n - 1) ? i + 1 : i];
        Vector3 segDir = Normalize(next - prev);
        Vector3 toCam  = Normalize(camPos - pts[i]);
        Vector3 side   = Cross(segDir, toCam);
        float len = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
        if (len < 1e-4f) side = { 1,0,0 }; else side = side * (1.0f / len);
        left[i]  = pts[i] + side * half;
        right[i] = pts[i] - side * half;
    }

    // セグメントごとに 2 三角形（TRIANGLELIST）
    for (int i = 0; i < n - 1; ++i) {
        float t0 = static_cast<float>(i)     / static_cast<float>(n - 1);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(n - 1);

        auto mk = [&](const Vector3& p, float u, float vCoord) {
            ProceduralMeshVertex out;
            out.position = p;
            out.texcoord = { u, vCoord };
            out.color    = color;
            out.age      = u;
            return out;
        };

        ProceduralMeshVertex L0 = mk(left[i],  t0, 0.0f);
        ProceduralMeshVertex R0 = mk(right[i], t0, 1.0f);
        ProceduralMeshVertex L1 = mk(left[i + 1],  t1, 0.0f);
        ProceduralMeshVertex R1 = mk(right[i + 1], t1, 1.0f);

        vertices_.push_back(L0); vertices_.push_back(R0); vertices_.push_back(R1);
        vertices_.push_back(L0); vertices_.push_back(R1); vertices_.push_back(L1);
    }
}

void LightningMesh::RebuildVertices()
{
    vertices_.clear();
    if (!camera_) return;

    // 形は flickerSeed_ から毎フレーム決定的に再生成する。カメラ向きリボンのため毎フレーム
    // 再構築するが、作業シードを基準へ戻すので flicker が起きるまで形は不変＝時間を止めれば静止する。
    seed_ = flickerSeed_;

    int levels = 1;
    while ((1 << levels) < param_.segments && levels < 7) ++levels;

    // --- 本線 ---
    std::vector<Vector3> main;
    main.push_back(start_);
    Subdivide(start_, end_, param_.jitter, levels, main);
    AppendBoltRibbon(main, param_.width, param_.color);

    // --- 枝 ---
    for (int bIdx = 0; bIdx < param_.branches; ++bIdx) {
        seed_ += 40503u;
        // 本線上のランダムな点から分岐
        int idx = 1 + static_cast<int>(Hash01(seed_) * (main.size() - 2));
        idx = (idx < 1) ? 1 : ((idx > static_cast<int>(main.size()) - 1) ? static_cast<int>(main.size()) - 1 : idx);
        Vector3 origin = main[idx];

        // 分岐方向：本線方向から少しずらす
        Vector3 dir = Normalize(end_ - start_);
        Vector3 ref = (std::fabs(dir.y) < 0.99f) ? Vector3{ 0,1,0 } : Vector3{ 1,0,0 };
        Vector3 u = Normalize(Cross(dir, ref));
        Vector3 v = Cross(dir, u);
        float ang = Hash01(seed_ * 3u + 1u) * 2.0f * kPi;
        Vector3 off = (u * std::cos(ang) + v * std::sin(ang));
        float blen = (0.3f + 0.4f * Hash01(seed_ * 11u)) * Length(end_ - start_);
        Vector3 bend = origin + (dir * 0.4f + off * 0.9f) * blen;

        std::vector<Vector3> branch;
        branch.push_back(origin);
        Subdivide(origin, bend, param_.branchJitter, levels - 1, branch);
        AppendBoltRibbon(branch, param_.width * 0.6f, param_.color);
    }

    UploadVertices(vertices_);
}

void LightningMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_ || !param_.isEnable) return;
    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

} // namespace YoRigine

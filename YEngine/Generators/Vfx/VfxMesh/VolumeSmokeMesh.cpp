// ===========================================================
// VolumeSmokeMesh.cpp
// ===========================================================
#include "VolumeSmokeMesh.h"
#include <cmath>

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

} // namespace YoRigine

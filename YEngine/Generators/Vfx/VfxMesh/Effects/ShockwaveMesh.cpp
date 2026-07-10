// ===========================================================
// ShockwaveMesh.cpp
// ===========================================================
#include "ShockwaveMesh.h"
#include "Systems/Camera/Camera.h"
#include <cmath>

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

} // namespace YoRigine

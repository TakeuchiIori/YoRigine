// ===========================================================
// LightVolumeMesh.cpp
// ===========================================================
#include "LightVolumeMesh.h"
#include <cmath>

namespace YoRigine {

// 頂点数: 6面 × 4頂点 + 縮退三角形 (面間接続) = 最大 6*4 + 5*2 = 34
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
    // age (uvZ) には各面の「Z 方向からの距離」を渡す
    // シェーダーが fadeZ 計算に使う

    // Front face (z = +1)
    { Vector3 c[4] = { Pnnp, Ppnp, Pnpp, Pppp }; AppendFace(c, col, 1.f); }
    // Back face  (z = -1)
    { Vector3 c[4] = { Ppnn, Pnnn, Pppn, Pnpn }; AppendFace(c, col, 1.f); }
    // Left face  (x = -1)
    { Vector3 c[4] = { Pnnn, Pnnp, Pnpn, Pnpp }; AppendFace(c, col, 0.5f); }
    // Right face (x = +1)
    { Vector3 c[4] = { Ppnp, Ppnn, Pppp, Pppn }; AppendFace(c, col, 0.5f); }
    // Bottom face (y = -1)
    { Vector3 c[4] = { Pnnn, Ppnn, Pnnp, Ppnp }; AppendFace(c, col, 0.5f); }
    // Top face   (y = +1)
    { Vector3 c[4] = { Pnpn, Pnpp, Pppn, Pppp }; AppendFace(c, col, 0.5f); }

    UploadVertices(vertices_);
}

// -----------------------------------------------------------
// 1面 (TriangleStrip 向け 4 頂点) を追加
//   corners[0]=左下, [1]=右下, [2]=左上, [3]=右上
// -----------------------------------------------------------
void LightVolumeMesh::AppendFace(const Vector3 corners[4],
                                  const Vector4& color,
                                  float          uvZ)
{
    // 縮退三角形で前の面と接続 (vertices_ が空でなければ)
    if (!vertices_.empty()) {
        vertices_.push_back(vertices_.back());       // 前の末尾を複製
        vertices_.push_back({});                     // ダミー (次の先頭と同じ座標で後ほど上書き)
    }

    // 4頂点: 左下, 右下, 左上, 右上 → TriangleStrip で 2 三角形
    const Vector2 uvs[4] = { {0,0}, {1,0}, {0,1}, {1,1} };

    for (int i = 0; i < 4; ++i)
    {
        ProceduralMeshVertex v;
        v.position = corners[i];
        v.texcoord = uvs[i];
        v.color    = color;
        v.age      = uvZ;  // シェーダー内で fadeZ として使用

        if (!vertices_.empty() && i == 0) {
            // 縮退の末尾ダミーをここで上書き
            vertices_.back() = v;
        }
        vertices_.push_back(v);
    }
}

// -----------------------------------------------------------
void LightVolumeMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_ || !param_.isEnable) return;

    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 4) return;

    // PSO / CBV は呼び出し元でセット済みを前提

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

} // namespace YoRigine

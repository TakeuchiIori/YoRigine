// ===========================================================
// TrailMesh.cpp
// ===========================================================
#include "TrailMesh.h"
#include <algorithm>
#include <cmath>

namespace YoRigine {

// -----------------------------------------------------------
void TrailMesh::Initialize(const TrailEffectParam& param)
{
    param_ = param;
    // 最大 maxPoints 点 × 2 頂点(tip/root) のストリップ
    const size_t maxVerts = static_cast<size_t>(param_.maxPoints) * 2;
    InitBuffer(maxVerts);
    vertices_.reserve(maxVerts);
}

// -----------------------------------------------------------
void TrailMesh::ApplyParam(const TrailEffectParam& param)
{
    param_ = param;
    // maxPoints が増えていたらバッファも拡張 (DynamicVertexBuffer が自動リサイズするので特別な処理不要)
}

// -----------------------------------------------------------
void TrailMesh::Clear()
{
    points_.clear();
    vertices_.clear();
    time_ = 0.f;
}

// -----------------------------------------------------------
void TrailMesh::AddPoint(const Vector3& tip, const Vector3& root)
{
    // maxPoints を超えたら古い端を削除
    while (static_cast<int>(points_.size()) >= param_.maxPoints) {
        points_.pop_back();
    }

    TrailPoint p;
    p.tip  = tip;
    p.root = root;
    p.age  = 0.f;
    points_.push_front(p);
}

// -----------------------------------------------------------
void TrailMesh::Update(float deltaTime)
{
    time_ += deltaTime;

    // 全ポイントの age を進める
    for (auto& p : points_) {
        p.age += deltaTime;
    }

    // 寿命切れを末尾から削除
    while (!points_.empty() && points_.back().age >= param_.lifetime) {
        points_.pop_back();
    }

    // 頂点を再構築して GPU へ
    RebuildVertices();
}

// -----------------------------------------------------------
void TrailMesh::RebuildVertices()
{
    vertices_.clear();

    const size_t n = points_.size();
    if (n < 2) {
        // 1点以下では面が作れない → 頂点ゼロで Upload して描画をスキップ
        UploadVertices(vertices_);
        return;
    }

    vertices_.reserve(n * 2);

    for (size_t i = 0; i < n; ++i)
    {
        const TrailPoint& p = points_[i];

        // texcoord.x : 0=tip(刃先), 1=root(根本)
        // texcoord.y : 0=最新ポイント, 1=最古ポイント (UV スクロール対応)
        const float tY = static_cast<float>(i) / static_cast<float>(n - 1);

        // 幅補間: 新しい端 → widthStart, 古い端 → widthEnd
        //   ※ 実際の幅は (tip - root) の長さで既に決まっているので
        //     ここでは色/フェード用に age を正規化して渡す
        const float normalizedAge = NormalizeAge(p.age);

        // 色グラデーション: colorStart(根元) ↔ colorEnd(先端) を lerp
        //   実際のブレンドはシェーダーが gMeshParam.colorInner/Outer で行う
        //   頂点カラーは乗算マスクとして白～フェード値を渡す
        const float fade = 1.f - normalizedAge; // 古いほど透明
        const Vector4 vColor = { 1.f, 1.f, 1.f, fade };

        // UV スクロール: texcoord.y にオフセット加算
        const float uvY = tY + time_ * param_.uvScrollSpeed;

        // Tip 頂点 (texcoord.x = 0)
        ProceduralMeshVertex vTip;
        vTip.position = p.tip;
        vTip.texcoord = { 0.f, uvY };
        vTip.color    = vColor;
        vTip.age      = normalizedAge;
        vertices_.push_back(vTip);

        // Root 頂点 (texcoord.x = 1)
        ProceduralMeshVertex vRoot;
        vRoot.position = p.root;
        vRoot.texcoord = { 1.f, uvY };
        vRoot.color    = vColor;
        vRoot.age      = normalizedAge;
        vertices_.push_back(vRoot);
    }

    UploadVertices(vertices_);
}

// -----------------------------------------------------------
void TrailMesh::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_) return;

    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 4) return; // 最低 2 ポイント = 4 頂点必要

    // PSO / ルートシグネチャ / CBV は呼び出し元でセット済みを前提

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

} // namespace YoRigine

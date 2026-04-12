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
        // --- 新規追加: パラメータの適用 ---
        shapeType_ = param_.shapeType;
        widthSegments_ = param_.widthSegments;
        arcAngleDeg_ = param_.arcAngleDeg;

        // 最大 maxPoints 点 × 2 頂点(tip/root) のストリップ
        const size_t maxVerts = static_cast<size_t>(param_.maxPoints) * 2;
        InitBuffer(maxVerts);
        vertices_.reserve(maxVerts);
    }

    // -----------------------------------------------------------
    void TrailMesh::ApplyParam(const TrailEffectParam& param)
    {
        param_ = param;
        shapeType_ = param_.shapeType;
        widthSegments_ = param_.widthSegments;
        arcAngleDeg_ = param_.arcAngleDeg;

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
        AddPoint(tip, root, Vector3{ 1.f, 0.f, 0.f }); // widthDir はデフォルトで X 軸
    }

    // -----------------------------------------------------------
    void TrailMesh::AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir)
    {
        // maxPoints を超えたら古い端を削除
        while (static_cast<int>(points_.size()) >= param_.maxPoints) {
            points_.pop_back();
        }

        TrailPoint p;
        p.tip = tip;
        p.root = root;
        p.widthDir = widthDir;
        p.age = 0.f;
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

        switch (shapeType_) {
        case TrailShapeType::Arc: RebuildArc(); break;
        case TrailShapeType::Fan: RebuildFan(); break;
        case TrailShapeType::Flat:
        default:                  RebuildFlat(); break;
        }

        UploadVertices(vertices_);
    }

    // -----------------------------------------------------------
    // Flat: 従来通り tip/root の 2 列ストリップ
    // -----------------------------------------------------------
    void TrailMesh::RebuildFlat()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        vertices_.reserve(n * 2);

        for (size_t i = 0; i < n; ++i)
        {
            const TrailPoint& p = points_[i];
            const float tY = static_cast<float>(i) / static_cast<float>(n - 1);
            const float normalizedAge = NormalizeAge(p.age);
            const Vector4 vColor = CalcFadeColor(normalizedAge);
            const float uvY = tY + time_ * param_.uvScrollSpeed;

            ProceduralMeshVertex vTip;
            vTip.position = p.tip;
            vTip.texcoord = { 0.f, uvY };
            vTip.color = vColor;
            vTip.age = normalizedAge;
            vertices_.push_back(vTip);

            ProceduralMeshVertex vRoot;
            vRoot.position = p.root;
            vRoot.texcoord = { 1.f, uvY };
            vRoot.color = vColor;
            vRoot.age = normalizedAge;
            vertices_.push_back(vRoot);
        }
    }

    // -----------------------------------------------------------
    // Arc: 各ポイントで断面を widthSegments 本の列に分割し
    //      tip→root 方向を弧状に曲げた断面を生成する
    //
    //  断面イメージ (widthSegments=4, arcAngleDeg=120 の場合):
    //
    //      tip
    //       |  ← 弧の中心軸 (tip-root ベクトル)
    //   col0 col1 col2 col3 col4
    //      root
    //
    //  各列の位置は tip-root 中点を中心に widthDir 方向へ弧を描く
    // -----------------------------------------------------------
    void TrailMesh::RebuildArc()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        const int   segs = std::max(1, widthSegments_);
        const float halfRad = (arcAngleDeg_ * 3.14159265f / 180.f) * 0.5f;

        // 頂点数: n ポイント × (segs+1) 列
        // TriangleStrip でポイント間を繋ぐので各セグメント列ごとにストリップを分ける
        // セグメント s と s+1 の列を交互に並べた n*2 頂点のストリップを segs 本作る

        for (int s = 0; s < segs; ++s)
        {
            // 縮退三角形でセグメント間を接続
            if (s > 0 && !vertices_.empty()) {
                vertices_.push_back(vertices_.back());
                vertices_.push_back({});  // ダミー (後で上書き)
            }

            bool firstInSeg = true;

            for (size_t i = 0; i < n; ++i)
            {
                const TrailPoint& p = points_[i];
                const float tY = static_cast<float>(i) / static_cast<float>(n - 1);
                const float normalizedAge = NormalizeAge(p.age);
                const Vector4 vColor = CalcFadeColor(normalizedAge);
                const float uvY = tY + time_ * param_.uvScrollSpeed;

                // tip-root の中点と半径
                const Vector3 center = {
                    (p.tip.x + p.root.x) * 0.5f,
                    (p.tip.y + p.root.y) * 0.5f,
                    (p.tip.z + p.root.z) * 0.5f
                };
                const float radius = std::sqrt(
                    (p.tip.x - center.x) * (p.tip.x - center.x) +
                    (p.tip.y - center.y) * (p.tip.y - center.y) +
                    (p.tip.z - center.z) * (p.tip.z - center.z));

                // セグメント s と s+1 の弧角度
                for (int col = s; col <= s + 1; ++col)
                {
                    const float t = static_cast<float>(col) / static_cast<float>(segs);
                    const float ang = -halfRad + t * (arcAngleDeg_ * 3.14159265f / 180.f);

                    // 弧上の位置: center + widthDir*sin(ang)*r + (tip-center)*cos(ang)
                    const float cosA = std::cos(ang);
                    const float sinA = std::sin(ang);

                    const Vector3 tipDir = {
                        (p.tip.x - center.x) / (radius + 1e-6f),
                        (p.tip.y - center.y) / (radius + 1e-6f),
                        (p.tip.z - center.z) / (radius + 1e-6f)
                    };

                    ProceduralMeshVertex v;
                    v.position = {
                        center.x + (tipDir.x * cosA + p.widthDir.x * sinA) * radius,
                        center.y + (tipDir.y * cosA + p.widthDir.y * sinA) * radius,
                        center.z + (tipDir.z * cosA + p.widthDir.z * sinA) * radius
                    };
                    v.texcoord = { t, uvY };
                    v.color = vColor;
                    v.age = normalizedAge;

                    // 縮退ダミーの上書き
                    if (s > 0 && firstInSeg && col == s) {
                        vertices_.back() = v;
                        firstInSeg = false;
                    }
                    vertices_.push_back(v);
                }
            }
        }
    }

    // -----------------------------------------------------------
    // Fan: tip を頂点とした扇形断面
    //      tip から root 方向に widthDir で広げる
    //
    //  断面イメージ (segs=3 の場合):
    //
    //       tip (固定)
    //      /  |  \
    //   r0   r1   r2   r3   ← root 側を分割
    // -----------------------------------------------------------
    void TrailMesh::RebuildFan()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        const int   segs = std::max(1, widthSegments_);
        const float halfRad = (arcAngleDeg_ * 3.14159265f / 180.f) * 0.5f;

        for (int s = 0; s < segs; ++s)
        {
            if (s > 0 && !vertices_.empty()) {
                vertices_.push_back(vertices_.back());
                vertices_.push_back({});
            }

            bool firstInSeg = true;

            for (size_t i = 0; i < n; ++i)
            {
                const TrailPoint& p = points_[i];
                const float tY = static_cast<float>(i) / static_cast<float>(n - 1);
                const float normalizedAge = NormalizeAge(p.age);
                const Vector4 vColor = CalcFadeColor(normalizedAge);
                const float uvY = tY + time_ * param_.uvScrollSpeed;

                // root 方向の単位ベクトル
                const Vector3 toRoot = {
                    p.root.x - p.tip.x,
                    p.root.y - p.tip.y,
                    p.root.z - p.tip.z
                };
                const float len = std::sqrt(
                    toRoot.x * toRoot.x + toRoot.y * toRoot.y + toRoot.z * toRoot.z) + 1e-6f;
                const Vector3 rootDir = { toRoot.x / len, toRoot.y / len, toRoot.z / len };

                for (int col = s; col <= s + 1; ++col)
                {
                    const float t = static_cast<float>(col) / static_cast<float>(segs);
                    const float ang = -halfRad + t * (arcAngleDeg_ * 3.14159265f / 180.f);

                    const float cosA = std::cos(ang);
                    const float sinA = std::sin(ang);

                    ProceduralMeshVertex v;
                    v.position = {
                        p.tip.x + (rootDir.x * cosA + p.widthDir.x * sinA) * len,
                        p.tip.y + (rootDir.y * cosA + p.widthDir.y * sinA) * len,
                        p.tip.z + (rootDir.z * cosA + p.widthDir.z * sinA) * len
                    };
                    v.texcoord = { t, uvY };
                    v.color = vColor;
                    v.age = normalizedAge;

                    if (s > 0 && firstInSeg && col == s) {
                        vertices_.back() = v;
                        firstInSeg = false;
                    }
                    vertices_.push_back(v);
                }
            }
        }
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
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
        case TrailShapeType::Custom: RebuildCustom(); break;
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

            // ★ 三日月カーブ: ageが0(最新)と1(最古)のときに0になり、0.5のときに1になるサイン波
            float widthScale = 1.0f;
            if (param_.crescentShape) {
                // sin(π * t) で両端が細くなる三日月型を作る
                widthScale = std::sin(normalizedAge * 3.14159265f);
            }

            // 中心点と方向ベクトルを計算
            Vector3 center = { (p.tip.x + p.root.x) * 0.5f, (p.tip.y + p.root.y) * 0.5f, (p.tip.z + p.root.z) * 0.5f };
            Vector3 toTip = { p.tip.x - center.x, p.tip.y - center.y, p.tip.z - center.z };

            // 幅をスケールして現在のTipとRootを計算
            Vector3 currentTip = { center.x + toTip.x * widthScale, center.y + toTip.y * widthScale, center.z + toTip.z * widthScale };
            Vector3 currentRoot = { center.x - toTip.x * widthScale, center.y - toTip.y * widthScale, center.z - toTip.z * widthScale };

            // 頂点の追加 (Tip側)
            ProceduralMeshVertex vTip;
            vTip.position = currentTip;
            vTip.texcoord = { 0.f, uvY };
            vTip.color = vColor;
            vTip.age = normalizedAge;
            vertices_.push_back(vTip);

            // 頂点の追加 (Root側)
            ProceduralMeshVertex vRoot;
            vRoot.position = currentRoot;
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

    void TrailMesh::RebuildCustom()
    {
        const auto& outline = param_.customVertices;
        if (outline.size() < 3) return; // 3点未満は面を作れない

        // 厚み（Z値の幅）
        float halfThick = param_.thickness * 0.5f;
        if (halfThick <= 0.001f) halfThick = 0.05f; // 最低限の厚み

        // 1. Ear Clipping（耳切り法）による 2Dポリゴンの三角形分割
        std::vector<int> indices;
        std::vector<int> remainingList;
        for (int i = 0; i < outline.size(); ++i) remainingList.push_back(i);

        int safety = 1000;
        while (remainingList.size() > 3 && safety-- > 0)
        {
            bool earFound = false;
            for (size_t i = 0; i < remainingList.size(); ++i) {
                int prev = remainingList[(i == 0) ? remainingList.size() - 1 : i - 1];
                int curr = remainingList[i];
                int next = remainingList[(i + 1) % remainingList.size()];

                Vector2 a = outline[prev];
                Vector2 b = outline[curr];
                Vector2 c = outline[next];

                // 外積で凸(Convex)か判定 (時計回り/反時計回りで符号が変わる)
                float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
                if (cross <= 0.0f) continue; // 凹んでいる部分はスキップ

                // 他の点がこの三角形の中に含まれていないかチェック
                bool isEar = true;
                for (int idx : remainingList) {
                    if (idx == prev || idx == curr || idx == next) continue;
                    if (IsPointInTriangle(outline[idx], a, b, c)) {
                        isEar = false;
                        break;
                    }
                }

                if (isEar) {
                    indices.push_back(prev);
                    indices.push_back(curr);
                    indices.push_back(next);
                    remainingList.erase(remainingList.begin() + i);
                    earFound = true;
                    break;
                }
            }
            if (!earFound) break; // 複雑すぎる交差などがあれば強制終了
        }
        // 最後の3点
        if (remainingList.size() == 3) {
            indices.push_back(remainingList[0]);
            indices.push_back(remainingList[1]);
            indices.push_back(remainingList[2]);
        }

        // 2. 頂点を GPU 用のフォーマットに変換し、Z方向の厚みをつける
        // ※今回は TriangleStrip ではなく、TriangleList として描画するため、
        // 頂点配列にそのまま三角形をポンポン入れていきます（後でTopologyの変更が必要）
        auto addTriangle = [&](const Vector3& p1, const Vector3& p2, const Vector3& p3, float uvX) {
            ProceduralMeshVertex v;
            v.color = { 1.f, 1.f, 1.f, 1.f };
            v.age = 0.f;

            v.position = p1; v.texcoord = { uvX, 0.f }; vertices_.push_back(v);
            v.position = p2; v.texcoord = { uvX, 1.f }; vertices_.push_back(v);
            v.position = p3; v.texcoord = { uvX, 0.5f }; vertices_.push_back(v);
            };

        // 表面 (+Z) と 裏面 (-Z)
        for (size_t i = 0; i < indices.size(); i += 3) {
            int i1 = indices[i], i2 = indices[i + 1], i3 = indices[i + 2];

            // 表面
            addTriangle(
                { outline[i1].x, outline[i1].y, halfThick },
                { outline[i2].x, outline[i2].y, halfThick },
                { outline[i3].x, outline[i3].y, halfThick }, 0.0f
            );
            // 裏面 (カリングされないように頂点順を逆にする)
            addTriangle(
                { outline[i1].x, outline[i1].y, -halfThick },
                { outline[i3].x, outline[i3].y, -halfThick },
                { outline[i2].x, outline[i2].y, -halfThick }, 1.0f
            );
        }

        // 側面 (フチの部分を四角形で繋ぐ)
        for (size_t i = 0; i < outline.size(); ++i) {
            int next = static_cast<int>((i + 1) % outline.size());
            Vector3 p1_f = { outline[i].x, outline[i].y, halfThick };
            Vector3 p2_f = { outline[next].x, outline[next].y, halfThick };
            Vector3 p1_b = { outline[i].x, outline[i].y, -halfThick };
            Vector3 p2_b = { outline[next].x, outline[next].y, -halfThick };

            // 四角形は2つの三角形
            addTriangle(p1_f, p2_f, p1_b, 0.5f);
            addTriangle(p1_b, p2_f, p2_b, 0.5f);
        }
    }

    bool TrailMesh::IsPointInTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c) const
    {
        auto crossProduct = [](const Vector2& v1, const Vector2& v2) { return v1.x * v2.y - v1.y * v2.x; };
        Vector2 ab = { b.x - a.x, b.y - a.y };
        Vector2 bc = { c.x - b.x, c.y - b.y };
        Vector2 ca = { a.x - c.x, a.y - c.y };
        Vector2 ap = { p.x - a.x, p.y - a.y };
        Vector2 bp = { p.x - b.x, p.y - b.y };
        Vector2 cp = { p.x - c.x, p.y - c.y };
        float c1 = crossProduct(ab, ap);
        float c2 = crossProduct(bc, bp);
        float c3 = crossProduct(ca, cp);
        return ((c1 >= 0.0f && c2 >= 0.0f && c3 >= 0.0f) || (c1 <= 0.0f && c2 <= 0.0f && c3 <= 0.0f));
    }

    // -----------------------------------------------------------
    void TrailMesh::Draw(ID3D12GraphicsCommandList* cmdList)
    {
        if (!isVisible_) return;

        const uint32_t vertCount = GetVertexCount();
        if (vertCount < 4) return; // 最低 2 ポイント = 4 頂点必要

        // PSO / ルートシグネチャ / CBV は呼び出し元でセット済みを前提
        if (shapeType_ == TrailShapeType::Custom) {
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        else {
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        }
        BindVertexBuffer(cmdList);
        cmdList->DrawInstanced(vertCount, 1, 0, 0);
    }

} // namespace YoRigine
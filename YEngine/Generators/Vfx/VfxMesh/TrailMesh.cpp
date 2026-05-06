// ===========================================================
// TrailMesh.cpp
// ★ UE Niagara Ribbon 参考の拡張
//   - Arc / Fan にも Catmull-Rom スムージング適用
//   - 幅ウェーブ (widthWaveAmp/Freq) による慣性感
//   - crescentShape を全 Shape タイプで統一
// ===========================================================
#include "TrailMesh.h"
#include <algorithm>
#include <cmath>

namespace YoRigine {

    static constexpr float kPi = 3.14159265f;

    // -----------------------------------------------------------
    // Catmull-Rom スプライン補間 (共通ヘルパー)
    // -----------------------------------------------------------
    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1,
                               const Vector3& p2, const Vector3& p3, float t)
    {
        float t2 = t * t;
        float t3 = t2 * t;
        return {
            0.5f * ((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3),
            0.5f * ((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3),
            0.5f * ((2*p1.z) + (-p0.z+p2.z)*t + (2*p0.z-5*p1.z+4*p2.z-p3.z)*t2 + (-p0.z+3*p1.z-3*p2.z+p3.z)*t3),
        };
    }

    // ポイント列を Catmull-Rom でスムージングして返す
    static std::vector<TrailPoint> SmoothPoints(const std::deque<TrailPoint>& pts, int subdivisions)
    {
        const size_t n = pts.size();
        if (n < 2) return {};

        const int steps = std::max(1, subdivisions);
        std::vector<TrailPoint> result;
        result.reserve((n - 1) * steps + 1);

        for (size_t i = 0; i < n - 1; ++i)
        {
            const TrailPoint& p0 = pts[(i == 0)     ? 0     : i - 1];
            const TrailPoint& p1 = pts[i];
            const TrailPoint& p2 = pts[i + 1];
            const TrailPoint& p3 = pts[(i + 2 >= n) ? n - 1 : i + 2];

            int count = (i == n - 2) ? steps + 1 : steps;
            for (int j = 0; j < count; ++j)
            {
                float t = static_cast<float>(j) / static_cast<float>(steps);

                TrailPoint sp;
                sp.tip      = CatmullRom(p0.tip,      p1.tip,      p2.tip,      p3.tip,      t);
                sp.root     = CatmullRom(p0.root,      p1.root,      p2.root,      p3.root,      t);
                sp.widthDir = CatmullRom(p0.widthDir,  p1.widthDir,  p2.widthDir,  p3.widthDir,  t);
                // widthDir を再正規化
                float len = std::sqrt(sp.widthDir.x*sp.widthDir.x + sp.widthDir.y*sp.widthDir.y + sp.widthDir.z*sp.widthDir.z);
                if (len > 1e-6f) { sp.widthDir.x/=len; sp.widthDir.y/=len; sp.widthDir.z/=len; }
                sp.age      = p1.age + (p2.age - p1.age) * t;
                result.push_back(sp);
            }
        }
        return result;
    }

    // -----------------------------------------------------------
    void TrailMesh::Initialize(const TrailEffectParam& param)
    {
        param_         = param;
        shapeType_     = param_.shapeType;
        widthSegments_ = param_.widthSegments;
        arcAngleDeg_   = param_.arcAngleDeg;

        const size_t maxVerts = static_cast<size_t>(param_.maxPoints) * 2 * std::max(1, param_.widthSegments);
        InitBuffer(maxVerts);
        vertices_.reserve(maxVerts);
    }

    // -----------------------------------------------------------
    void TrailMesh::ApplyParam(const TrailEffectParam& param)
    {
        param_         = param;
        shapeType_     = param_.shapeType;
        widthSegments_ = param_.widthSegments;
        arcAngleDeg_   = param_.arcAngleDeg;
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
        AddPoint(tip, root, Vector3{ 1.f, 0.f, 0.f });
    }

    void TrailMesh::AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir)
    {
        while (static_cast<int>(points_.size()) >= param_.maxPoints) {
            points_.pop_back();
        }
        TrailPoint p;
        p.tip      = tip;
        p.root     = root;
        p.widthDir = widthDir;
        p.age      = 0.f;
        points_.push_front(p);
    }

    // -----------------------------------------------------------
    void TrailMesh::Update(float deltaTime)
    {
        time_ += deltaTime;

        for (auto& p : points_) p.age += deltaTime;

        while (!points_.empty() && points_.back().age >= param_.lifetime) {
            points_.pop_back();
        }

        RebuildVertices();
    }

    // -----------------------------------------------------------
    void TrailMesh::RebuildVertices()
    {
        vertices_.clear();
        switch (shapeType_) {
        case TrailShapeType::Arc:    RebuildArc();    break;
        case TrailShapeType::Fan:    RebuildFan();    break;
        case TrailShapeType::Custom: RebuildCustom(); break;
        case TrailShapeType::Flat:
        default:                     RebuildFlat();   break;
        }
        UploadVertices(vertices_);
    }

    // -----------------------------------------------------------
    // ★ 幅スケールの計算 (全 Shape で共通)
    //   crescentShape: サイン曲線で中間を太く
    //   widthWave:     サイン波で微細な幅ゆらぎ
    // -----------------------------------------------------------
    float TrailMesh::CalcWidthScale(float normalizedAge, float tY) const
    {
        float scale = 1.0f;

        // 先端→根本にかけて width を lerp
        float widthRatio = (param_.widthStart > 1e-6f)
            ? lerp(1.0f, param_.widthEnd / param_.widthStart, normalizedAge)
            : 0.f;
        scale *= widthRatio;

        if (param_.crescentShape) {
            // 中間 (age=0.5) で最も太い三日月型
            scale *= std::sin(normalizedAge * kPi);
        }

        // ★ 幅ウェーブ (UE の Sine-over-life 相当)
        if (param_.widthWaveAmp > 0.001f) {
            float wave = std::sin(tY * param_.widthWaveFreq * kPi * 2.0f) * param_.widthWaveAmp;
            scale *= (1.0f + wave);
        }

        return std::max(scale, 0.0f);
    }

    // -----------------------------------------------------------
    // Flat (★ Catmull-Rom スムージング済み)
    // -----------------------------------------------------------
    void TrailMesh::RebuildFlat()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        auto smoothed = SmoothPoints(points_, std::max(1, param_.splineSubdivisions));
        const size_t sn = smoothed.size();
        if (sn < 2) return;

        vertices_.reserve(sn * 2);

        for (size_t i = 0; i < sn; ++i)
        {
            const TrailPoint& p = smoothed[i];
            const float tY            = static_cast<float>(i) / static_cast<float>(sn - 1);
            const float normalizedAge = NormalizeAge(p.age);
            const Vector4 vColor      = CalcFadeColor(normalizedAge);
            const float uvY           = tY + time_ * param_.uvScrollSpeed;
            const float widthScale    = CalcWidthScale(normalizedAge, tY);

            Vector3 center = {
                (p.tip.x + p.root.x) * 0.5f,
                (p.tip.y + p.root.y) * 0.5f,
                (p.tip.z + p.root.z) * 0.5f
            };
            Vector3 toTip = {
                p.tip.x - center.x,
                p.tip.y - center.y,
                p.tip.z - center.z
            };

            Vector3 currentTip = {
                center.x + toTip.x * widthScale,
                center.y + toTip.y * widthScale,
                center.z + toTip.z * widthScale
            };
            Vector3 currentRoot = {
                center.x - toTip.x * widthScale,
                center.y - toTip.y * widthScale,
                center.z - toTip.z * widthScale
            };

            ProceduralMeshVertex vTip;
            vTip.position = currentTip;
            vTip.texcoord = { 0.f, uvY };
            vTip.color    = vColor;
            vTip.age      = normalizedAge;
            vertices_.push_back(vTip);

            ProceduralMeshVertex vRoot;
            vRoot.position = currentRoot;
            vRoot.texcoord = { 1.f, uvY };
            vRoot.color    = vColor;
            vRoot.age      = normalizedAge;
            vertices_.push_back(vRoot);
        }
    }

    // -----------------------------------------------------------
    // ★ Arc (Catmull-Rom スムージング追加)
    // -----------------------------------------------------------
    void TrailMesh::RebuildArc()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        auto smoothed = SmoothPoints(points_, std::max(1, param_.splineSubdivisions));
        const size_t sn = smoothed.size();
        if (sn < 2) return;

        const int   segs    = std::max(1, widthSegments_);
        const float halfRad = (arcAngleDeg_ * kPi / 180.f) * 0.5f;

        for (int s = 0; s < segs; ++s)
        {
            if (s > 0 && !vertices_.empty()) {
                vertices_.push_back(vertices_.back());
                vertices_.push_back({});
            }

            bool firstInSeg = true;

            for (size_t i = 0; i < sn; ++i)
            {
                const TrailPoint& p = smoothed[i];
                const float tY            = static_cast<float>(i) / static_cast<float>(sn - 1);
                const float normalizedAge = NormalizeAge(p.age);
                const Vector4 vColor      = CalcFadeColor(normalizedAge);
                const float uvY           = tY + time_ * param_.uvScrollSpeed;
                const float widthScale    = CalcWidthScale(normalizedAge, tY);

                // tip-root ベクトル
                Vector3 toRoot = {
                    p.root.x - p.tip.x,
                    p.root.y - p.tip.y,
                    p.root.z - p.tip.z
                };
                const float tipRootLen = std::sqrt(
                    toRoot.x*toRoot.x + toRoot.y*toRoot.y + toRoot.z*toRoot.z) + 1e-6f;
                const Vector3 rootDir = {
                    toRoot.x / tipRootLen,
                    toRoot.y / tipRootLen,
                    toRoot.z / tipRootLen
                };

                const Vector3 midPoint = {
                    (p.tip.x + p.root.x) * 0.5f,
                    (p.tip.y + p.root.y) * 0.5f,
                    (p.tip.z + p.root.z) * 0.5f
                };

                for (int col = s; col <= s + 1; ++col)
                {
                    const float tX  = static_cast<float>(col) / static_cast<float>(segs);
                    const float ang = -halfRad + tX * (arcAngleDeg_ * kPi / 180.f);

                    const float cosA = std::cos(ang);
                    const float sinA = std::sin(ang);
                    const float r    = tipRootLen * 0.5f * widthScale;

                    ProceduralMeshVertex v;
                    v.position = {
                        midPoint.x + (rootDir.x * cosA + p.widthDir.x * sinA) * r,
                        midPoint.y + (rootDir.y * cosA + p.widthDir.y * sinA) * r,
                        midPoint.z + (rootDir.z * cosA + p.widthDir.z * sinA) * r
                    };
                    v.texcoord = { tX, uvY };
                    v.color    = vColor;
                    v.age      = normalizedAge;

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
    // ★ Fan (Catmull-Rom スムージング追加)
    // -----------------------------------------------------------
    void TrailMesh::RebuildFan()
    {
        const size_t n = points_.size();
        if (n < 2) return;

        auto smoothed = SmoothPoints(points_, std::max(1, param_.splineSubdivisions));
        const size_t sn = smoothed.size();
        if (sn < 2) return;

        const int   segs    = std::max(1, widthSegments_);
        const float halfRad = (arcAngleDeg_ * kPi / 180.f) * 0.5f;

        for (int s = 0; s < segs; ++s)
        {
            if (s > 0 && !vertices_.empty()) {
                vertices_.push_back(vertices_.back());
                vertices_.push_back({});
            }

            bool firstInSeg = true;

            for (size_t i = 0; i < sn; ++i)
            {
                const TrailPoint& p = smoothed[i];
                const float tY            = static_cast<float>(i) / static_cast<float>(sn - 1);
                const float normalizedAge = NormalizeAge(p.age);
                const Vector4 vColor      = CalcFadeColor(normalizedAge);
                const float uvY           = tY + time_ * param_.uvScrollSpeed;
                const float widthScale    = CalcWidthScale(normalizedAge, tY);

                const Vector3 toRoot = {
                    p.root.x - p.tip.x,
                    p.root.y - p.tip.y,
                    p.root.z - p.tip.z
                };
                const float len = std::sqrt(
                    toRoot.x*toRoot.x + toRoot.y*toRoot.y + toRoot.z*toRoot.z) + 1e-6f;
                const Vector3 rootDir = {
                    toRoot.x / len,
                    toRoot.y / len,
                    toRoot.z / len
                };

                for (int col = s; col <= s + 1; ++col)
                {
                    const float tX  = static_cast<float>(col) / static_cast<float>(segs);
                    const float ang = -halfRad + tX * (arcAngleDeg_ * kPi / 180.f);

                    const float cosA = std::cos(ang);
                    const float sinA = std::sin(ang);

                    ProceduralMeshVertex v;
                    v.position = {
                        p.tip.x + (rootDir.x * cosA + p.widthDir.x * sinA) * len * widthScale,
                        p.tip.y + (rootDir.y * cosA + p.widthDir.y * sinA) * len * widthScale,
                        p.tip.z + (rootDir.z * cosA + p.widthDir.z * sinA) * len * widthScale
                    };
                    v.texcoord = { tX, uvY };
                    v.color    = vColor;
                    v.age      = normalizedAge;

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
    // Custom (変更なし)
    // -----------------------------------------------------------
    void TrailMesh::RebuildCustom()
    {
        const auto& outline = param_.customVertices;
        if (outline.size() < 3) return;

        float halfThick = param_.thickness * 0.5f;
        if (halfThick <= 0.001f) halfThick = 0.05f;

        std::vector<int> indices;
        std::vector<int> remainingList;
        for (int i = 0; i < (int)outline.size(); ++i) remainingList.push_back(i);

        int safety = 1000;
        while (remainingList.size() > 3 && safety-- > 0)
        {
            bool earFound = false;
            for (size_t i = 0; i < remainingList.size(); ++i) {
                int prev = remainingList[(i == 0) ? remainingList.size() - 1 : i - 1];
                int curr = remainingList[i];
                int next = remainingList[(i + 1) % remainingList.size()];

                Vector2 a = outline[prev], b = outline[curr], c = outline[next];
                float cross = (b.x-a.x)*(c.y-b.y) - (b.y-a.y)*(c.x-b.x);
                if (cross <= 0.0f) continue;

                bool isEar = true;
                for (int idx : remainingList) {
                    if (idx == prev || idx == curr || idx == next) continue;
                    if (IsPointInTriangle(outline[idx], a, b, c)) { isEar = false; break; }
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
            if (!earFound) break;
        }
        if (remainingList.size() == 3) {
            indices.push_back(remainingList[0]);
            indices.push_back(remainingList[1]);
            indices.push_back(remainingList[2]);
        }

        auto addTriangle = [&](const Vector3& p1, const Vector3& p2, const Vector3& p3, float uvX) {
            ProceduralMeshVertex v;
            v.color = { 1.f, 1.f, 1.f, 1.f };
            v.age   = 0.f;
            v.position = p1; v.texcoord = { uvX, 0.f };   vertices_.push_back(v);
            v.position = p2; v.texcoord = { uvX, 1.f };   vertices_.push_back(v);
            v.position = p3; v.texcoord = { uvX, 0.5f };  vertices_.push_back(v);
        };

        for (size_t i = 0; i < indices.size(); i += 3) {
            int i1 = indices[i], i2 = indices[i+1], i3 = indices[i+2];
            addTriangle({ outline[i1].x, outline[i1].y,  halfThick },
                        { outline[i2].x, outline[i2].y,  halfThick },
                        { outline[i3].x, outline[i3].y,  halfThick }, 0.0f);
            addTriangle({ outline[i1].x, outline[i1].y, -halfThick },
                        { outline[i3].x, outline[i3].y, -halfThick },
                        { outline[i2].x, outline[i2].y, -halfThick }, 1.0f);
        }
        for (size_t i = 0; i < outline.size(); ++i) {
            int next = static_cast<int>((i + 1) % outline.size());
            addTriangle({ outline[i].x,    outline[i].y,     halfThick },
                        { outline[next].x, outline[next].y,  halfThick },
                        { outline[i].x,    outline[i].y,    -halfThick }, 0.5f);
            addTriangle({ outline[i].x,    outline[i].y,    -halfThick },
                        { outline[next].x, outline[next].y,  halfThick },
                        { outline[next].x, outline[next].y, -halfThick }, 0.5f);
        }
    }

    // -----------------------------------------------------------
    bool TrailMesh::IsPointInTriangle(const Vector2& p, const Vector2& a,
                                       const Vector2& b, const Vector2& c) const
    {
        auto cross = [](const Vector2& v1, const Vector2& v2) {
            return v1.x * v2.y - v1.y * v2.x;
        };
        Vector2 ab{b.x-a.x, b.y-a.y}, bc{c.x-b.x, c.y-b.y}, ca{a.x-c.x, a.y-c.y};
        Vector2 ap{p.x-a.x, p.y-a.y}, bp{p.x-b.x, p.y-b.y}, cp{p.x-c.x, p.y-c.y};
        float c1 = cross(ab,ap), c2 = cross(bc,bp), c3 = cross(ca,cp);
        return ((c1>=0&&c2>=0&&c3>=0)||(c1<=0&&c2<=0&&c3<=0));
    }

    // -----------------------------------------------------------
    void TrailMesh::Draw(ID3D12GraphicsCommandList* cmdList)
    {
        if (!isVisible_) return;
        const uint32_t vertCount = GetVertexCount();
        if (vertCount < 4) return;

        if (shapeType_ == TrailShapeType::Custom) {
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        } else {
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        }
        BindVertexBuffer(cmdList);
        cmdList->DrawInstanced(vertCount, 1, 0, 0);
    }

} // namespace YoRigine

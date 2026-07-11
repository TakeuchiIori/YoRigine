#include "GPUParticle.hlsli"
#include "../Method/Random.hlsli"

// 定数バッファ
ConstantBuffer<EmitterCommon> g_EmitterCommon : register(b0);
ConstantBuffer<EmitterSphere> g_EmitterSphere : register(b1);
ConstantBuffer<EmitterBox> g_EmitterBox : register(b2);
ConstantBuffer<EmitterTriangle> g_EmitterTriangle : register(b3);
ConstantBuffer<EmitterCone> g_EmitterCone : register(b4);
ConstantBuffer<EmitterMesh> g_EmitterMesh : register(b5);
ConstantBuffer<PerFrame> g_PerFrame : register(b6);
ConstantBuffer<ParticleParameters> g_ParticleParams : register(b7);

// UAVバッファ（パーティクルデータ：SoA hot/warm/cold）
RWStructuredBuffer<ParticleHot>  g_Hot  : register(u0);
RWStructuredBuffer<int> g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList     : register(u2);
RWStructuredBuffer<uint> g_ActiveCount  : register(u3);
RWStructuredBuffer<ParticleWarm> g_Warm : register(u4);
RWStructuredBuffer<ParticleCold> g_Cold : register(u5);

// SRVバッファ（メッシュ三角形データ）
StructuredBuffer<MeshTriangle> g_MeshTriangles : register(t0);

//=============================================================================
// groupshared（スレッドグループ内で共有）
//=============================================================================

/// <summary>
/// スレッドグループ内で生成されたパーティクル数
/// 最後に 1 回だけ g_ActiveCount に加算する
/// </summary>
groupshared uint g_GroupEmitCount;

//=============================================================================
// メッシュエミッター専用関数
//=============================================================================

/// <summary>
/// 累積面積による重み付けランダム選択で三角形を選択
/// </summary>
uint SelectTriangleByArea(inout RandomGenerator generator, uint triangleCount)
{
    if (triangleCount == 0)
    {
        return 0;
    }

    // 総面積を計算
    float totalArea = 0.0f;
    [loop]
    for (uint i = 0; i < triangleCount; i++)
    {
        totalArea += g_MeshTriangles[i].area;
    }

    // 面積に基づいてランダムに選択
    float randomValue = generator.Generated1d() * totalArea;
    float accumulatedArea = 0.0f;

    [loop]
    for (uint i = 0; i < triangleCount; i++)
    {
        accumulatedArea += g_MeshTriangles[i].area;
        if (randomValue <= accumulatedArea)
        {
            return i;
        }
    }

    // フォールバック
    return triangleCount - 1;
}

/// <summary>
/// メッシュ表面上のランダムな点を生成
/// </summary>
float3 GenerateRandomPointOnMeshSurface(inout RandomGenerator generator, uint triangleCount)
{
    if (triangleCount == 0)
    {
        return float3(0, 0, 0);
    }

    // 面積による重み付けで三角形を選択
    uint selectedTriangle = SelectTriangleByArea(generator, triangleCount);

    // 選択された三角形内のランダムな点を生成
    float u = generator.Generated1d();
    float v = generator.Generated1d();

    // 三角形の外に出ないように調整
    if (u + v > 1.0f)
    {
        u = 1.0f - u;
        v = 1.0f - v;
    }

    // 重心座標で点を計算
    float3 localPoint = BarycentricInterpolation(
        g_MeshTriangles[selectedTriangle].v0,
        g_MeshTriangles[selectedTriangle].v1,
        g_MeshTriangles[selectedTriangle].v2,
        u, v
    );

    // トランスフォーム適用
    return TransformPoint(
        localPoint,
        g_EmitterMesh.scale,
        g_EmitterMesh.rotation,
        g_EmitterMesh.translate
    );
}

/// <summary>
/// メッシュ内部のランダムな点を生成
/// </summary>
float3 GenerateRandomPointInMeshVolume(inout RandomGenerator generator, uint triangleCount)
{
    if (triangleCount == 0)
    {
        return g_EmitterMesh.translate;
    }

    // 表面上の点を取得
    float3 surfacePoint = GenerateRandomPointOnMeshSurface(generator, triangleCount);

    // メッシュの中心へのベクトル
    float3 toCenter = g_EmitterMesh.translate - surfacePoint;

    // ランダムな深さで内側へ移動
    float randomDepth = generator.Generated1d();

    return surfacePoint + toCenter * randomDepth;
}

/// <summary>
/// メッシュのエッジに沿った点を生成
/// </summary>
float3 GenerateRandomPointOnMeshEdge(inout RandomGenerator generator, uint triangleCount)
{
    if (triangleCount == 0)
    {
        return g_EmitterMesh.translate;
    }

    // ランダムに三角形を選択
    uint selectedTriangle = (uint) (generator.Generated1d() * float(triangleCount)) % triangleCount;

    MeshTriangle tri = g_MeshTriangles[selectedTriangle];
    
    // 有効なエッジのリストを作成
    uint validEdges[3];
    uint validCount = 0;
    
    if (tri.activeEdges & 1)
        validEdges[validCount++] = 0; // v0-v1
    if (tri.activeEdges & 2)
        validEdges[validCount++] = 1; // v1-v2
    if (tri.activeEdges & 4)
        validEdges[validCount++] = 2; // v2-v0

    if (validCount == 0)
        return float3(0, 0, 0); // エッジなし（通常ありえないが）

    // 有効なエッジの中からランダムに選択
    uint r = (uint) (generator.Generated1d() * float(validCount)) % validCount;
    uint edgeIndex = validEdges[r];

    float3 p0, p1;
    if (edgeIndex == 0)
    {
        p0 = tri.v0;
        p1 = tri.v1;
    }
    else if (edgeIndex == 1)
    {
        p0 = tri.v1;
        p1 = tri.v2;
    }
    else
    {
        p0 = tri.v2;
        p1 = tri.v0;
    }

    // 線形補間
    float t = generator.Generated1d();
    float3 localPoint = lerp(p0, p1, t);

    // トランスフォーム適用
    return TransformPoint(
        localPoint,
        g_EmitterMesh.scale,
        g_EmitterMesh.rotation,
        g_EmitterMesh.translate
    );
}

/// <summary>
/// メッシュの法線方向を取得（最も近い三角形の法線）
/// </summary>
float3 GetMeshNormalDirection(inout RandomGenerator generator, float3 particlePos, uint triangleCount)
{
    if (triangleCount == 0)
    {
        return float3(0, 1, 0);
    }

    float minDistSq = 1e10f;
    uint closestTriangle = 0;

    uint checkCount = min(triangleCount, 1000000u);

    [loop]
    for (uint i = 0; i < checkCount; i++)
    {
        float3 center = CalculateTriangleCenter(
            g_MeshTriangles[i].v0,
            g_MeshTriangles[i].v1,
            g_MeshTriangles[i].v2
        );

        // トランスフォーム適用した中心位置
        float3 transformedCenter = TransformPoint(
            center,
            g_EmitterMesh.scale,
            g_EmitterMesh.rotation,
            g_EmitterMesh.translate
        );

        float distSq = LengthSquared(particlePos - transformedCenter);

        if (distSq < minDistSq)
        {
            minDistSq = distSq;
            closestTriangle = i;
        }
    }

    // 法線を回転適用
    float3 normal = TransformDirection(
        g_MeshTriangles[closestTriangle].normal,
        g_EmitterMesh.rotation
    );

    // 法線方向にランダムなバリエーションを追加
    float3 randomVariation = (generator.Generated3d() * 2.0f - 1.0f) * 0.3f;

    return normalize(normal + randomVariation);
}

//=============================================================================
// 汎用位置生成関数
//=============================================================================

/// <summary>
/// エミッター形状に応じた位置を生成
/// </summary>
float3 GenerateParticlePosition(inout RandomGenerator generator, uint emitterShape)
{
    switch (emitterShape)
    {
        case EMITTER_SHAPE_SPHERE:
            return GenerateRandomPositionInSphere(
            generator,
            g_EmitterSphere.translate,
            g_EmitterSphere.radius
        );

        case EMITTER_SHAPE_BOX:
            return GenerateRandomPositionInBox(
            generator,
            g_EmitterBox.translate,
            g_EmitterBox.size
        );

        case EMITTER_SHAPE_TRIANGLE:
            return GenerateRandomPositionInTriangle(
            generator,
            g_EmitterTriangle.v1,
            g_EmitterTriangle.v2,
            g_EmitterTriangle.v3
        );

        case EMITTER_SHAPE_CONE:
            return GenerateRandomPositionInCone(
            generator,
            g_EmitterCone.translate,
            g_EmitterCone.direction,
            g_EmitterCone.radius,
            g_EmitterCone.height
        );

        case EMITTER_SHAPE_MESH:
        {
                uint mode = g_EmitterMesh.emitMode;
                uint triCount = g_EmitterMesh.triangleCount;

                if (mode == MESH_EMIT_MODE_SURFACE)
                {
                    return GenerateRandomPointOnMeshSurface(generator, triCount);
                }
                else if (mode == MESH_EMIT_MODE_VOLUME)
                {
                    return GenerateRandomPointInMeshVolume(generator, triCount);
                }
                else
                {
                // MESH_EMIT_MODE_EDGE
                    return GenerateRandomPointOnMeshEdge(generator, triCount);
                }
            }

        default:
            return float3(0, 0, 0);
    }
}

/// <summary>
/// エミッター形状に応じた方向を生成
/// </summary>
float3 GenerateParticleDirection(inout RandomGenerator generator, uint emitterShape, float3 particlePos)
{
    switch (emitterShape)
    {
        case EMITTER_SHAPE_SPHERE:
        {
                float3 direction = particlePos - g_EmitterSphere.translate;
                return normalize(direction);
            }

        case EMITTER_SHAPE_BOX:
        {
                return normalize(generator.Generated3d() * 2.0f - 1.0f);
            }

        case EMITTER_SHAPE_TRIANGLE:
        {
                float3 edge1 = g_EmitterTriangle.v2 - g_EmitterTriangle.v1;
                float3 edge2 = g_EmitterTriangle.v3 - g_EmitterTriangle.v1;
                float3 normal = normalize(cross(edge1, edge2));
                float3 randomVariation = (generator.Generated3d() * 2.0f - 1.0f) * 0.3f;
                return normalize(normal + randomVariation);
            }

        case EMITTER_SHAPE_CONE:
        {
                float3 baseDirection = g_EmitterCone.direction;
                float3 randomVariation = (generator.Generated3d() * 2.0f - 1.0f) * 0.5f;
                return normalize(baseDirection + randomVariation);
            }

        case EMITTER_SHAPE_MESH:
        {
                return GetMeshNormalDirection(
                generator,
                particlePos,
                g_EmitterMesh.triangleCount
            );
            }

        default:
            return float3(0, 1, 0);
    }
}

//=============================================================================
// エミッション制御
//=============================================================================

/// <summary>
/// エミッターが有効かどうかと、パーティクル数を取得
/// </summary>
bool GetEmitInfo(out float particleCount)
{
    particleCount = 0;

    switch (g_EmitterCommon.emitterShape)
    {
        case EMITTER_SHAPE_SPHERE:
            particleCount = g_EmitterSphere.count;
            return g_EmitterSphere.isEmit != 0;

        case EMITTER_SHAPE_BOX:
            particleCount = g_EmitterBox.count;
            return g_EmitterBox.isEmit != 0;

        case EMITTER_SHAPE_TRIANGLE:
            particleCount = g_EmitterTriangle.count;
            return g_EmitterTriangle.isEmit != 0;

        case EMITTER_SHAPE_CONE:
            particleCount = g_EmitterCone.count;
            return g_EmitterCone.isEmit != 0;

        case EMITTER_SHAPE_MESH:
            particleCount = g_EmitterMesh.count;
            return g_EmitterMesh.isEmit != 0;

        default:
            return false;
    }
}

/// <summary>
/// GenerateDirection（未使用なら消してもOK）
/// </summary>
float3 GenerateDirection(inout RandomGenerator generator, uint emitterShape, float3 emitterPos, float3 particlePos)
{
    switch (emitterShape)
    {
        case EMITTER_SHAPE_SPHERE:
        {
                return normalize(particlePos - emitterPos);
            }
        case EMITTER_SHAPE_BOX:
        {
                return normalize(generator.Generated3d() * 2.0f - 1.0f);
            }
        case EMITTER_SHAPE_TRIANGLE:
        {
                float3 edge1 = g_EmitterTriangle.v2 - g_EmitterTriangle.v1;
                float3 edge2 = g_EmitterTriangle.v3 - g_EmitterTriangle.v1;
                float3 normal = normalize(cross(edge1, edge2));
                return normal + (generator.Generated3d() * 2.0f - 1.0f) * 0.3f;
            }
        case EMITTER_SHAPE_CONE:
        {
                float3 baseDirection = g_EmitterCone.direction;
                float3 randomVariation = (generator.Generated3d() * 2.0f - 1.0f) * 0.5f;
                return normalize(baseDirection + randomVariation);
            }
        default:
        {
                return normalize(generator.Generated3d() * 2.0f - 1.0f);
            }
    }
}

//=============================================================================
// メイン（Emit）
//=============================================================================

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    // グループ内生成数の初期化（スレッド0のみ）
    if (GTid.x == 0)
    {
        g_GroupEmitCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint index = DTid.x;

    // エミッション情報を取得
    float particleCount;
    bool shouldEmit = GetEmitInfo(particleCount);
    uint emitCount = (uint) particleCount;

    // エミッションが無効、または範囲外の場合は処理しない
    if (!shouldEmit || index >= emitCount)
    {
        return;
    }

    // ランダムジェネレーターを初期化
    RandomGenerator generator;
    generator.seed = GenerateParticleSeed(index, emitCount, g_PerFrame.time);

    // FreeList からインデックスを取得（アトミック操作）
    int freeListIndex;
    InterlockedAdd(g_FreeListIndex[0], -1, freeListIndex);

    // 有効なインデックスかチェック
    if (freeListIndex <= 0 || freeListIndex >= (int) kMaxParticles)
    {
        // 無効な場合は元に戻す（エラーケースなので頻度は低い想定）
        InterlockedAdd(g_FreeListIndex[0], 1);
        return;
    }

    // パーティクルインデックスを取得
    uint particleIndex = g_FreeList[freeListIndex];

    // パーティクルインデックスが有効範囲内かチェック
    if (particleIndex >= kMaxParticles)
    {
        // FreeListIndex だけ元に戻す
        InterlockedAdd(g_FreeListIndex[0], 1);
        return;
    }

    /*==============================================================================================//

                                         パーティクルを初期化

    //==============================================================================================*/

    ParticleHot  hot  = (ParticleHot) 0;
    ParticleWarm warm = (ParticleWarm) 0;
    ParticleCold cold = (ParticleCold) 0;

    // 位置生成
    float3 particlePosition = GenerateParticlePosition(generator, g_EmitterCommon.emitterShape);
    hot.translate = particlePosition;

    // 生存時間（ランダム幅適用）
    hot.lifeTime = GetRandomValue(
        generator,
        g_ParticleParams.lifeTime,
        g_ParticleParams.lifeTimeVariance
    );
    hot.currentTime = 0.0f;

    // スケール（ランダム幅適用）。scale は保存せず VS で導出
    warm.startScale = GetRandomVector3(
        generator, g_ParticleParams.startScale, g_ParticleParams.startScaleVariance);
    warm.endScale = GetRandomVector3(
        generator, g_ParticleParams.endScale, g_ParticleParams.endScaleVariance);

    // 回転（ランダム幅適用）
    hot.rotate = GetRandomValue(
        generator,
        g_ParticleParams.rotation,
        g_ParticleParams.rotationVariance
    );

    // 色（ランダム幅適用）。color は保存せず VS で導出。startColor.a のみクランプ
    warm.startColor = GetRandomColor(
        generator, g_ParticleParams.startColor, g_ParticleParams.startColorVariance);
    warm.startColor.a = saturate(warm.startColor.a);
    warm.endColor = GetRandomColor(
        generator, g_ParticleParams.endColor, g_ParticleParams.endColorVariance);

    // 方向生成
    float3 direction = GenerateParticleDirection(
        generator,
        g_EmitterCommon.emitterShape,
        particlePosition
    );

    // 速度ベース（ランダム幅適用）
    float3 velocityBase = GetRandomVector3(
        generator,
        g_ParticleParams.velocity,
        g_ParticleParams.velocityVariance
    );

    float speed = length(velocityBase);

    if (speed > 0.0001f)
    {
        // 速度パラメータが設定されている場合は、方向とミックス
        float3 mixedDir = normalize(direction + normalize(velocityBase) * 0.5f);
        hot.velocity = mixedDir * speed;
    }
    else
    {
        // 速度パラメータが0の場合は、方向のみを使用
        hot.velocity = direction * 0.1f;
    }

    // アクティブ化
    hot.isActive = 1;
    cold.isParent = 1;
    cold.lastTranslate = particlePosition;

    // SoA バッファへ書き込み
    g_Hot[particleIndex]  = hot;
    g_Warm[particleIndex] = warm;
    g_Cold[particleIndex] = cold;

    // グループ内生成数をインクリメント（groupshared に対してのみ）
    InterlockedAdd(g_GroupEmitCount, 1);

    // グループ内の全スレッドが更新し終わるのを待つ
    GroupMemoryBarrierWithGroupSync();

    // グループ内で 1 回だけ g_ActiveCount を増やす
    if (GTid.x == 0 && g_GroupEmitCount > 0)
    {
        InterlockedAdd(g_ActiveCount[0], g_GroupEmitCount);
    }
}

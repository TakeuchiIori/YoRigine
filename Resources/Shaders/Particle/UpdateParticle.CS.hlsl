#include "GPUParticle.hlsli"

// Update は Hot のみ触る（scale/color は保存せず VS で導出）。SoA の帯域利得はここが本体。
RWStructuredBuffer<ParticleHot>  g_Hot        : register(u0);
ConstantBuffer<PerFrame>         g_PerFrame   : register(b0);
ConstantBuffer<ParticleParameters> g_ParticleParams : register(b1);
RWStructuredBuffer<int>  g_FreeListIndex      : register(u1);
RWStructuredBuffer<uint> g_FreeList           : register(u2);
RWStructuredBuffer<uint> g_ActiveCount        : register(u3);
StructuredBuffer<GpuForceField> g_ForceFields : register(t0);
// インダイレクト描画: 生存粒子のスロット番号だけを詰める。描画は生存数ぶんだけになる。
RWStructuredBuffer<uint> g_DrawList : register(u6);
RWStructuredBuffer<uint> g_DrawArgs : register(u7); // [1]=InstanceCount 兼 生存数カウンタ

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint baseIndex = DTid.x * kParticlesPerThread;

    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleIndex = baseIndex + i;

        if (particleIndex >= kMaxParticles)
            break;

        ParticleHot h = g_Hot[particleIndex];

        if (h.isActive == 0)
            continue;

        // --- 生存判定 ---
        if (h.currentTime < h.lifeTime)
        {
            // トレイル（子パーティクル）生成は SpawnTrailParticle.CS.hlsl の別パスで行う。
            // このカーネル内で FreeList の Pop（子生成）と Push（死亡処理）を同時に行うと
            // 競合でスロット喪失・カウント破壊が起きるため、Pop はここでは絶対にしない。

            // --- 共通物理更新 ---
            float lifeRatio = saturate(h.currentTime / h.lifeTime);
            h.velocity.y -= g_ParticleParams.gravity * g_PerFrame.deltaTime;

            // --- フォースフィールド適用 ---
            for (uint f = 0; f < g_PerFrame.forceFieldCount; f++)
            {
                GpuForceField field = g_ForceFields[f];
                if (field.isEnable == 0) continue;

                float3 toCenter = field.center - h.translate;
                float  dist     = length(toCenter);

                // 範囲内判定
                bool inside;
                if (field.shape == FIELD_SHAPE_SPHERE) {
                    inside = (dist <= field.radius);
                } else {
                    float3 delta = abs(h.translate - field.center);
                    inside = (delta.x <= field.halfExtents.x &&
                              delta.y <= field.halfExtents.y &&
                              delta.z <= field.halfExtents.z);
                }
                if (!inside) continue;

                // 加速方向
                float3 accelDir;
                if (field.mode == FIELD_MODE_DIRECTIONAL) {
                    accelDir = normalize(field.direction);
                } else if (field.mode == FIELD_MODE_CONVERGE) {
                    accelDir = (dist > 0.001f) ? (toCenter / dist) : float3(0, 0, 0);
                } else { // REPEL
                    accelDir = (dist > 0.001f) ? -(toCenter / dist) : float3(0, 0, 0);
                }

                // 距離減衰
                float falloffScale = 1.0f;
                if (field.falloff > 0.0001f) {
                    float normDist = (field.shape == FIELD_SHAPE_SPHERE)
                        ? saturate(dist / max(0.001f, field.radius))
                        : saturate(length(abs(h.translate - field.center)
                                         / max(float3(0.001f, 0.001f, 0.001f), field.halfExtents)));
                    falloffScale = lerp(1.0f, normDist, field.falloff);
                }

                float3 accel = accelDir * field.strength * falloffScale;

                // 螺旋収束（Converge 時のみ）
                if (field.mode == FIELD_MODE_CONVERGE && field.spiralStrengthMax > 0.0001f) {
                    uint   fieldSeed   = f * 0x9e3779b9u;
                    float3 randomAxis  = HashToUnitVector(particleIndex ^ fieldSeed);
                    float3 spiralAxis  = normalize(lerp(float3(0, 1, 0), randomAxis, field.randomAxisBlend));
                    float3 tangent     = normalize(cross(accelDir, spiralAxis));
                    float  spiralAmt   = lerp(field.spiralStrengthMin, field.spiralStrengthMax,
                                              Hash11(particleIndex ^ fieldSeed ^ 0x517cc1b7u));

                    // orbitHoldRatio: 寿命の途中まで周回し、その後収束へ遷移
                    float effectiveHold = field.orbitHoldRatio;
                    if (field.approachVariance > 0.0001f) {
                        effectiveHold += Hash11(particleIndex ^ fieldSeed ^ 0xdeadbeefu) * field.approachVariance;
                    }
                    float radialScale = smoothstep(0.0f, max(0.001f, effectiveHold), lifeRatio);
                    float3 spiralAccel = tangent * field.strength * spiralAmt * falloffScale;
                    accel = lerp(spiralAccel, accelDir * field.strength * falloffScale, radialScale);
                }

                h.velocity += accel * g_PerFrame.deltaTime;

                // 最大速度クランプ
                if (field.maxSpeed > 0.0001f) {
                    float sp = length(h.velocity);
                    if (sp > field.maxSpeed) {
                        h.velocity *= field.maxSpeed / sp;
                    }
                }

                // killRadius: 到達したら寿命満了扱いで消滅
                if (field.mode == FIELD_MODE_CONVERGE && field.killRadius > 0.0001f && dist <= field.killRadius) {
                    h.currentTime = h.lifeTime;
                }
            }

            h.translate += h.velocity * g_PerFrame.deltaTime;
            h.rotate += g_ParticleParams.rotationSpeed * g_PerFrame.deltaTime;
            h.currentTime += g_PerFrame.deltaTime;

            g_Hot[particleIndex] = h; // 書き戻し

            // この粒子は今フレームも生存 → インダイレクト描画リストへ追記。
            // 描画インスタンス数 = ここで数えた生存数（死粒は描かない）。
            // ※Emit/Trail で今フレーム新規発生した粒子は次フレームの Update で拾われる（1F遅延・不可視）。
            uint drawIdx;
            InterlockedAdd(g_DrawArgs[1], 1, drawIdx);
            g_DrawList[drawIdx] = particleIndex;
        }
        else
        {
            // ==========================================================
            // 死亡処理（Push処理）
            // ==========================================================
            h.isActive = 0;
            g_Hot[particleIndex] = h;

            int originalCount;
            // 現在の在庫数を増やし、加算前の「空きスロット番号」を取得
            InterlockedAdd(g_FreeListIndex[0], 1, originalCount);

            // 修正：originalCount（加算前）がそのまま書き込み先インデックスになる
            if (originalCount >= 0 && originalCount < (int) kMaxParticles)
            {
                g_FreeList[originalCount] = particleIndex;
            }
            else
            {
                // エラー回避：もし満杯ならカウントを戻す
                InterlockedAdd(g_FreeListIndex[0], -1);
            }

            InterlockedAdd(g_ActiveCount[0], (uint) -1);
        }
    }
}
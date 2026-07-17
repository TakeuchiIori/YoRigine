#include "GPUParticle.hlsli"

// トレイル（子パーティクル）生成専用パス。
// UpdateParticle.CS（死亡処理 = FreeList への Push）と同一ディスパッチ内で
// Pop を行うと FreeList が競合で壊れるため、UAV バリアを挟んだ別パスとして実行する。
// このカーネルは Pop のみを行う（Push は一切しない）。
// ルートシグネチャは ParticleUpdateCS と共用（warm=u4 / cold=u5 を追加済み）。

RWStructuredBuffer<ParticleHot>  g_Hot        : register(u0);
ConstantBuffer<PerFrame>         g_PerFrame   : register(b0);
ConstantBuffer<ParticleParameters> g_ParticleParams : register(b1);
RWStructuredBuffer<int>  g_FreeListIndex      : register(u1);
RWStructuredBuffer<uint> g_FreeList           : register(u2);
RWStructuredBuffer<uint> g_ActiveCount        : register(u3);
RWStructuredBuffer<ParticleWarm> g_Warm       : register(u4);
RWStructuredBuffer<ParticleCold> g_Cold       : register(u5);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (g_ParticleParams.child.isTrail == 0)
        return;
    if (g_ParticleParams.child.minDistance <= 0.0f)
        return;

    uint baseIndex = DTid.x * kParticlesPerThread;

    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleIndex = baseIndex + i;

        if (particleIndex >= kMaxParticles)
            break;

        ParticleHot  ph = g_Hot[particleIndex];
        ParticleCold pc = g_Cold[particleIndex];

        // 親の生きているパーティクルのみが子（トレイル）を生成する
        if (ph.isActive == 0)
            continue;
        if (pc.isParent == 0)
            continue;

        float3 currentPos = ph.translate;
        float3 lastPos    = pc.lastTranslate;
        float3 delta      = currentPos - lastPos;
        float  d          = length(delta);

        if (d < g_ParticleParams.child.minDistance)
            continue;

        // 親の現在色/スケールは保存していないので warm から lifeRatio で導出（VS と同一式）
        ParticleWarm pw = g_Warm[particleIndex];
        float parentRatio = (ph.lifeTime > 0.0f) ? saturate(ph.currentTime / ph.lifeTime) : 0.0f;
        float4 parentColor = lerp(pw.startColor, pw.endColor, parentRatio);
        float3 parentScale = lerp(pw.startScale, pw.endScale, parentRatio);

        // 高速移動時は minDistance 刻みで補間しつつ複数生成
        int steps = (int) (d / g_ParticleParams.child.minDistance);
        if (steps < 1)
            steps = 1;
        // 1粒あたりの生成暴走ガード（初回フレームの異常距離などで FreeList を食い潰さないため）
        if (steps > 32)
            steps = 32;

        float3 stepVec      = delta / (float) steps;
        float3 spawnBasePos = lastPos;

        for (int s = 0; s < steps; s++)
        {
            spawnBasePos += stepVec;
            uint emitPerStep = (uint) g_ParticleParams.child.emissionCount;

            for (uint j = 0; j < emitPerStep; j++)
            {
                // FreeList から Pop（このパスに Push は存在しないため競合しない）
                int originalCount;
                InterlockedAdd(g_FreeListIndex[0], -1, originalCount);

                if (originalCount > 0)
                {
                    uint childIdx = g_FreeList[originalCount - 1];

                    // 子の Hot/Warm/Cold を組み立て（isActive は最後に書く）
                    ParticleHot  ch = (ParticleHot) 0;
                    ParticleWarm cw = (ParticleWarm) 0;
                    ParticleCold cc = (ParticleCold) 0;

                    ch.translate   = spawnBasePos;
                    ch.velocity    = float3(0, 0, 0);
                    ch.rotate      = ph.rotate;
                    ch.currentTime = 0.0f;
                    ch.lifeTime    = g_ParticleParams.child.lifeTime;

                    cw.startColor  = parentColor;
                    cw.endColor    = pw.endColor;
                    float3 baseScale = (g_ParticleParams.child.inheritScale == 1) ? parentScale : float3(1, 1, 1);
                    cw.startScale  = baseScale * g_ParticleParams.child.startScale;
                    cw.endScale    = cw.startScale * g_ParticleParams.child.endScale;

                    cc.isParent      = 0;
                    cc.lastTranslate = spawnBasePos;

                    g_Warm[childIdx] = cw;
                    g_Cold[childIdx] = cc;
                    ch.isActive = 1;
                    g_Hot[childIdx] = ch;  // isActive=1 を含む Hot を最後に書き、初期化途中スロットの誤認を防ぐ

                    InterlockedAdd(g_ActiveCount[0], 1);
                }
                else
                {
                    // 在庫切れ: カウントを戻して終了
                    InterlockedAdd(g_FreeListIndex[0], 1);
                    s = steps;
                    break;
                }
            }
        }

        // 親の lastTranslate を更新
        pc.lastTranslate = currentPos;
        g_Cold[particleIndex] = pc;
    }
}

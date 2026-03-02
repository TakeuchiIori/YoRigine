#include "GPUParticle.hlsli"

RWStructuredBuffer<Particle> g_Particles : register(u0);
ConstantBuffer<PerFrame> g_PerFrame : register(b0);
ConstantBuffer<ParticleParameters> g_ParticleParams : register(b1);

RWStructuredBuffer<int> g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList : register(u2);
RWStructuredBuffer<uint> g_ActiveCount : register(u3);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint baseIndex = DTid.x * kParticlesPerThread;
    
    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleIndex = baseIndex + i;
        
        if (particleIndex >= kMaxParticles)
            break;
        
        if (g_Particles[particleIndex].isActive == 0)
            continue;
        
        // --- 生存判定 ---
        if (g_Particles[particleIndex].currentTime < g_Particles[particleIndex].lifeTime)
        {
            // ==========================================================
            // トレイル（子パーティクル）生成ロジック
            // ==========================================================
            if (g_Particles[particleIndex].isParent == 1 && g_ParticleParams.child.isTrail == 1)
            {
                float3 currentPos = g_Particles[particleIndex].translate;
                float3 lastPos = g_Particles[particleIndex].lastTranslate;
                float3 delta = currentPos - lastPos;
                float d = length(delta);

                if (d >= g_ParticleParams.child.minDistance && g_ParticleParams.child.minDistance > 0.0f)
                {
                    int steps = (int) (d / g_ParticleParams.child.minDistance);
                    if (steps < 1)
                        steps = 1;

                    float3 stepVec = delta / (float) steps;
                    float3 spawnBasePos = lastPos;

                    for (int s = 0; s < steps; s++)
                    {
                        spawnBasePos += stepVec;
                        uint emitPerStep = (uint) g_ParticleParams.child.emissionCount;
                        
                        for (uint j = 0; j < emitPerStep; j++)
                        {
                            // 【修正：Pop処理】
                            int originalCount;
                            InterlockedAdd(g_FreeListIndex[0], -1, originalCount);

                            // 取り出す前の在庫が 1 以上あれば取得可能
                            if (originalCount > 0)
                            {
                                // originalCount は 1～N なので、-1 して 0～N-1 のインデックスを得る
                                uint childIdx = g_FreeList[originalCount - 1];

                                // 子の初期化
                                g_Particles[childIdx].isActive = 1;
                                g_Particles[childIdx].isParent = 0;
                                g_Particles[childIdx].translate = spawnBasePos;
                                g_Particles[childIdx].lastTranslate = spawnBasePos;
                                g_Particles[childIdx].velocity = float3(0, 0, 0);
                                g_Particles[childIdx].rotate = g_Particles[particleIndex].rotate;
                                g_Particles[childIdx].currentTime = 0.0f;
                                g_Particles[childIdx].lifeTime = g_ParticleParams.child.lifeTime;
                                
                                g_Particles[childIdx].startColor = g_Particles[particleIndex].color;
                                g_Particles[childIdx].endColor = g_Particles[particleIndex].endColor;
                                g_Particles[childIdx].color = g_Particles[childIdx].startColor;

                                float3 baseScale = (g_ParticleParams.child.inheritScale == 1) ? g_Particles[particleIndex].scale : float3(1, 1, 1);
                                g_Particles[childIdx].startScale = baseScale * g_ParticleParams.child.startScale;
                                g_Particles[childIdx].endScale = g_Particles[childIdx].startScale * g_ParticleParams.child.endScale;
                                g_Particles[childIdx].scale = g_Particles[childIdx].startScale;

                                InterlockedAdd(g_ActiveCount[0], 1);
                            }
                            else
                            {
                                // 在庫切れなのでカウントを戻して終了
                                InterlockedAdd(g_FreeListIndex[0], 1);
                                s = steps; // 外側のループも抜ける
                                break;
                            }
                        }
                    }
                    g_Particles[particleIndex].lastTranslate = currentPos;
                }
            }

            // --- 共通物理更新 ---
            float lifeRatio = saturate(g_Particles[particleIndex].currentTime / g_Particles[particleIndex].lifeTime);
            g_Particles[particleIndex].velocity.y -= g_ParticleParams.gravity * g_PerFrame.deltaTime;
            g_Particles[particleIndex].translate += g_Particles[particleIndex].velocity * g_PerFrame.deltaTime;
            g_Particles[particleIndex].rotate += g_ParticleParams.rotationSpeed * g_PerFrame.deltaTime;
            g_Particles[particleIndex].scale = lerp(g_Particles[particleIndex].startScale, g_Particles[particleIndex].endScale, lifeRatio);
            g_Particles[particleIndex].color = lerp(g_Particles[particleIndex].startColor, g_Particles[particleIndex].endColor, lifeRatio);
            
            g_Particles[particleIndex].currentTime += g_PerFrame.deltaTime;
        }
        else
        {
            // ==========================================================
            // 死亡処理（Push処理）
            // ==========================================================
            g_Particles[particleIndex].isActive = 0;
            g_Particles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
            
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
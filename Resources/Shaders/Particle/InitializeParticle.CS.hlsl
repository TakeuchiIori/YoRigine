#include "GPUParticle.hlsli"

RWStructuredBuffer<Particle> g_Particles : register(u0);
RWStructuredBuffer<int> g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList : register(u2);
RWStructuredBuffer<uint> g_ActiveCount : register(u3);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // ---------------------------------------------------------
    // 1. インデックスとカウントの初期化（全スレッドの先頭のみ）
    // ---------------------------------------------------------
    if (DTid.x == 0)
    {
        // 修正ポイント：kMaxParticles そのものを入れる。
        // Pop(取り出し)時に「元の数 - 1」を参照するため、
        // 2,000,000個あるなら2,000,000と設定するのが正解です。
        g_FreeListIndex[0] = kMaxParticles;
        g_ActiveCount[0] = 0;
    }
    
    // 全グループで共有するメモリではないため、バリアは不要です。
    // (DTid.x == 0 の書き込みは完了したものとして扱われます)
    
    uint baseIndex = DTid.x * kParticlesPerThread;
    
    // ---------------------------------------------------------
    // 2. 各スレッドが担当するパーティクルの初期化
    // ---------------------------------------------------------
    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleIndex = baseIndex + i;
        
        if (particleIndex < kMaxParticles)
        {
            // パーティクルの完全クリア
            g_Particles[particleIndex].translate = float3(0, 0, 0);
            g_Particles[particleIndex].lastTranslate = float3(0, 0, 0);
            g_Particles[particleIndex].scale = float3(0, 0, 0);
            g_Particles[particleIndex].startScale = float3(0, 0, 0);
            g_Particles[particleIndex].endScale = float3(0, 0, 0);
            g_Particles[particleIndex].rotate = 0.0f;
            g_Particles[particleIndex].velocity = float3(0, 0, 0);
            g_Particles[particleIndex].lifeTime = 0.0f;
            g_Particles[particleIndex].currentTime = 0.0f;
            g_Particles[particleIndex].color = float4(1, 1, 1, 0);
            g_Particles[particleIndex].isActive = 0;
            g_Particles[particleIndex].isParent = 0;
            g_Particles[particleIndex].isBillboard = 1;

            // FreeListに自分のIDを登録（これで「空き席」が埋まる）
            g_FreeList[particleIndex] = particleIndex;
        }
    }
}
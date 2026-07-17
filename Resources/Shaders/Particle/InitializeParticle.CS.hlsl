#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleHot>  g_Hot  : register(u0);
RWStructuredBuffer<int>  g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList      : register(u2);
RWStructuredBuffer<uint> g_ActiveCount   : register(u3);
RWStructuredBuffer<ParticleWarm> g_Warm : register(u4);
RWStructuredBuffer<ParticleCold> g_Cold : register(u5);

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
            // Hot のクリア
            ParticleHot h = (ParticleHot) 0;
            g_Hot[particleIndex] = h;

            // Warm のクリア
            ParticleWarm w = (ParticleWarm) 0;
            g_Warm[particleIndex] = w;

            // Cold のクリア
            ParticleCold c = (ParticleCold) 0;
            g_Cold[particleIndex] = c;

            // FreeListに自分のIDを登録（これで「空き席」が埋まる）
            g_FreeList[particleIndex] = particleIndex;
        }
    }
}
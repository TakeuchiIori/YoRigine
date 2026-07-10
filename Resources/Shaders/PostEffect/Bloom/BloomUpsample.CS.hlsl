#include "../Common/PostEffectCS.hlsli"

// =============================================================================
// Dual Kawase ブルーム: アップサンプル段
// -----------------------------------------------------------------------------
// 下位（低解像度）ミップ gLower を 3x3 テントフィルタで拡大し、対象ミップ
// gOutput へ加算合成する（read-add-write）。ピラミッドを下から上へ積み上げ、
// 各段の柔らかいブラーを重ねることで広く滑らかなグローを作る。
// filterRadius は下位ミップのテクセル単位でにじみ半径を制御する。
// =============================================================================
cbuffer BloomUpParams : register(b0)
{
    float filterRadius; // にじみ半径 [下位ミップのテクセル]
    float3 _padUp;
};

Texture2D<float4>   gLower   : register(t0);
SamplerState        gSampler : register(s0);
RWTexture2D<float4> gOutput  : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    // にじみ半径は下位ミップのテクセル間隔基準
    uint lw, lh;
    gLower.GetDimensions(lw, lh);
    float2 r = filterRadius * float2(rcp(float(lw)), rcp(float(lh)));

    // 3x3 テント (1 2 1 / 2 4 2 / 1 2 1) / 16
    float3 s;
    s  = gLower.SampleLevel(gSampler, uv + float2(-r.x,  r.y), 0).rgb * 1.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( 0.0f, r.y), 0).rgb * 2.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( r.x,  r.y), 0).rgb * 1.0f;
    s += gLower.SampleLevel(gSampler, uv + float2(-r.x, 0.0f), 0).rgb * 2.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( 0.0f,0.0f), 0).rgb * 4.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( r.x, 0.0f), 0).rgb * 2.0f;
    s += gLower.SampleLevel(gSampler, uv + float2(-r.x,-r.y), 0).rgb * 1.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( 0.0f,-r.y), 0).rgb * 2.0f;
    s += gLower.SampleLevel(gSampler, uv + float2( r.x, -r.y), 0).rgb * 1.0f;
    s *= rcp(16.0f);

    float4 cur = gOutput[dtid.xy];
    gOutput[dtid.xy] = float4(cur.rgb + s, cur.a);
}

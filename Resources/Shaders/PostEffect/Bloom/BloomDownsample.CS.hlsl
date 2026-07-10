#include "../Common/PostEffectCS.hlsli"

// =============================================================================
// Dual Kawase ブルーム: ダウンサンプル段
// -----------------------------------------------------------------------------
// 上位（高解像度）ミップを 13 タップ（COD/Jimenez 方式）で 1/2 解像度へ縮小する。
// mip0 生成時のみ doThreshold=1 で高輝度抽出（BrightPass）を兼ねる。
// 深いミップ（doThreshold=0）は純粋な縮小のみで、しきい値で二度エネルギーを
// 削らないようにする。HDR リニアのまま出力（saturate しない）。
// =============================================================================
cbuffer BloomDownParams : register(b0)
{
    float threshold;    // 輝度しきい値（HDR基準）
    float doThreshold;  // 1.0 = BrightPass あり / 0.0 = 縮小のみ
    float2 _padDown;
};

Texture2D<float4>   gInput   : register(t0);
SamplerState        gSampler : register(s0);
RWTexture2D<float4> gOutput  : register(u0);

float3 Prefilter(float3 c)
{
    if (doThreshold < 0.5f) return c;
    float lum = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    float excess = max(0.0f, lum - threshold);
    return c * (excess / max(lum, 1e-4f));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    // サンプルするのは入力（上位ミップ）のテクセル間隔
    uint sw, sh;
    gInput.GetDimensions(sw, sh);
    float2 t = float2(rcp(float(sw)), rcp(float(sh)));

    // 13 タップ ダウンサンプル（中央 4 点を重く、周囲を軽く）
    float3 a = gInput.SampleLevel(gSampler, uv + t * float2(-2, -2), 0).rgb;
    float3 b = gInput.SampleLevel(gSampler, uv + t * float2( 0, -2), 0).rgb;
    float3 c = gInput.SampleLevel(gSampler, uv + t * float2( 2, -2), 0).rgb;
    float3 d = gInput.SampleLevel(gSampler, uv + t * float2(-2,  0), 0).rgb;
    float3 e = gInput.SampleLevel(gSampler, uv + t * float2( 0,  0), 0).rgb;
    float3 f = gInput.SampleLevel(gSampler, uv + t * float2( 2,  0), 0).rgb;
    float3 g = gInput.SampleLevel(gSampler, uv + t * float2(-2,  2), 0).rgb;
    float3 hh= gInput.SampleLevel(gSampler, uv + t * float2( 0,  2), 0).rgb;
    float3 i = gInput.SampleLevel(gSampler, uv + t * float2( 2,  2), 0).rgb;

    float3 j = gInput.SampleLevel(gSampler, uv + t * float2(-1, -1), 0).rgb;
    float3 k = gInput.SampleLevel(gSampler, uv + t * float2( 1, -1), 0).rgb;
    float3 l = gInput.SampleLevel(gSampler, uv + t * float2(-1,  1), 0).rgb;
    float3 m = gInput.SampleLevel(gSampler, uv + t * float2( 1,  1), 0).rgb;

    // 中央 4 点(j,k,l,m)=0.5、辺・角のブロックにそれぞれ 0.125 相当を配分
    float3 sum = (j + k + l + m) * 0.5f;
    sum += (a + b + d + e) * 0.125f;
    sum += (b + c + e + f) * 0.125f;
    sum += (d + e + g + hh) * 0.125f;
    sum += (e + f + hh + i) * 0.125f;
    sum *= 0.25f;

    gOutput[dtid.xy] = float4(Prefilter(sum), 1.0f);
}

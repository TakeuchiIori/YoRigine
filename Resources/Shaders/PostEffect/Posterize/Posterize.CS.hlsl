#include "../Common/PostEffectCS.hlsli"

cbuffer PosterizeParams : register(b0)
{
    int steps;
    float saturationBoost;
    float2 padding;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float3 RGBtoHSV(float3 rgb)
{
    float4 K = float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10f;
    return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0f - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0f, 1.0f), hsv.y);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float4 original = gInput.SampleLevel(gSampler, uv, 0);
    float3 c = original.rgb;

    float fSteps = float(max(steps, 2));
    c = floor(c * fSteps + 0.5f) / fSteps;

    float3 hsv = RGBtoHSV(c);
    hsv.y = saturate(hsv.y * saturationBoost);
    c = HSVtoRGB(hsv);

    // ポストエフェクトチェーンの中間バッファは HDR リニア。
    // ここで saturate / sRGB 変換すると後段 Bloom が色と輝度を正しく判定できないため、
    // HDR 値を保ったまま次のエフェクトへ渡す。表示変換は最終 blit だけで行う。
    c = max(c, 0.0f);
    gOutput[dtid.xy] = float4(c, original.a);
}

#include "../Common/PostEffectCS.hlsli"

// =============================================================================
// Dual Kawase ブルーム: 合成段
// -----------------------------------------------------------------------------
// フル解像度のシーン gScene に、積み上げたブルームピラミッドの最上位 gBloom
// （1/2 解像度）をバイリニア拡大しながら intensity で加算する。colorTemperature
// で滲みを暖色/寒色に寄せる。HDR リニアのまま出力し、トーンマップ＆sRGB は
// 最終 blit(CopyImage.PS) に一元化。
// =============================================================================
cbuffer BloomCompositeParams : register(b0)
{
    float intensity;        // ブルームの強さ
    float colorTemperature; // 暖色(+) / 寒色(-)
    float2 _padComp;
};

Texture2D<float4>   gScene   : register(t0);
Texture2D<float4>   gBloom   : register(t1);
SamplerState        gSampler : register(s0);
RWTexture2D<float4> gOutput  : register(u0);

float3 ApplyColorTemp(float3 bloom, float temp)
{
    bloom.r = max(0.0f, bloom.r + temp * 0.18f);
    bloom.g = max(0.0f, bloom.g + temp * 0.04f);
    bloom.b = max(0.0f, bloom.b - temp * 0.12f);
    return bloom;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float4 scene = gScene.SampleLevel(gSampler, uv, 0);
    float3 bloom = gBloom.SampleLevel(gSampler, uv, 0).rgb;
    bloom = ApplyColorTemp(bloom, colorTemperature);

    float3 result = scene.rgb + bloom * intensity;
    gOutput[dtid.xy] = float4(result, scene.a);
}

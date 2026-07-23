#include "../Common/PostEffectCS.hlsli"

cbuffer BloomParams : register(b0)
{
    float threshold;
    float intensity;
    float spread;
    float colorTemperature;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 ExtractBright(float3 c)
{
    float lum = Luminance(c);
    float excess = max(0.0f, lum - threshold);
    float factor = excess / max(lum, 0.0001f);
    return c * factor;
}

float3 ApplyColorTemp(float3 bloom, float temp)
{
    bloom.r = saturate(bloom.r + temp * 0.18f);
    bloom.g = saturate(bloom.g + temp * 0.04f);
    bloom.b = saturate(bloom.b - temp * 0.12f);
    return bloom;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float2 texelSize = float2(rcp(float(w)), rcp(float(h)));

    float4 original = gInput.SampleLevel(gSampler, uv, 0);

    const float PI2_8 = 3.14159265f * 2.0f / 8.0f;
    float3 bloom = float3(0, 0, 0);
    float totalWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float angle = PI2_8 * float(i);
        float2 uv1 = uv + float2(cos(angle), sin(angle)) * texelSize * spread;
        float2 uv2 = uv + float2(cos(angle), sin(angle)) * texelSize * spread * 2.0f;

        float3 s1 = gInput.SampleLevel(gSampler, uv1, 0).rgb;
        float3 s2 = gInput.SampleLevel(gSampler, uv2, 0).rgb;

        bloom += ExtractBright(s1) * 0.6f;
        bloom += ExtractBright(s2) * 0.3f;
        totalWeight += 0.9f;
    }

    bloom += ExtractBright(original.rgb) * 0.5f;
    totalWeight += 0.5f;

    bloom /= totalWeight;
    bloom = ApplyColorTemp(bloom, colorTemperature);

    float3 result = saturate(original.rgb + bloom * intensity);
    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, original.a);
}

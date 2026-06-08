#include "../Common/PostEffectCS.hlsli"

cbuffer ColorGradeParams : register(b0)
{
    float3 shadowColor;
    float  splitBalance;

    float3 highlightColor;
    float  splitStrength;

    float vibrance;
    float colorTemp;
    float colorTint;
    float padding;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

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

    float lum = Luminance(c);
    float shadowW    = 1.0f - smoothstep(0.0f, splitBalance + 0.25f, lum);
    float highlightW = smoothstep(splitBalance - 0.25f, 1.0f, lum);

    c += shadowColor    * shadowW    * splitStrength;
    c += highlightColor * highlightW * splitStrength;

    {
        float3 hsv = RGBtoHSV(c);
        float vibranceBoost = (1.0f - hsv.y) * vibrance * 0.6f;
        hsv.y = saturate(hsv.y + vibranceBoost);
        c = HSVtoRGB(hsv);
    }

    c.r = saturate(c.r + colorTemp * 0.12f);
    c.g = saturate(c.g + colorTemp * 0.03f);
    c.b = saturate(c.b - colorTemp * 0.15f);
    c.g = saturate(c.g - colorTint * 0.10f);
    c.r = saturate(c.r + colorTint * 0.05f);
    c.b = saturate(c.b + colorTint * 0.05f);

    c = saturate(c);
    c = LinearToSRGB(c);
    gOutput[dtid.xy] = float4(c, original.a);
}

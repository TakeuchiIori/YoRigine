#include "../Common/PostEffectCS.hlsli"

cbuffer ColorAdjustParams : register(b0)
{
    float brightness;
    float contrast;
    float saturation;
    float hue;
};

cbuffer ToneParams : register(b1)
{
    float gamma;
    float exposure;
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

float3 ApplyColorAdjust(float3 color)
{
    color += brightness;
    color = (color - 0.5f) * contrast + 0.5f;

    float3 hsv = RGBtoHSV(color);
    hsv.x += hue / 360.0f;
    hsv.x = frac(hsv.x + 1.0f);
    hsv.y *= saturation;
    hsv.y = clamp(hsv.y, 0.0f, 1.0f);
    return HSVtoRGB(hsv);
}

float3 ApplyToneAdjust(float3 color)
{
    color *= pow(2.0f, exposure);
    color = pow(abs(color), 1.0f / gamma);
    return color;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float4 originalColor = gInput.SampleLevel(gSampler, uv, 0);

    float3 c = ApplyColorAdjust(originalColor.rgb);
    c = ApplyToneAdjust(c);
    c = clamp(c, 0.0f, 1.0f);

    c = LinearToSRGB(c);
    gOutput[dtid.xy] = float4(c, originalColor.a);
}

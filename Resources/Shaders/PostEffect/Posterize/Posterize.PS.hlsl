#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer PosterizeParams : register(b0)
{
    int   steps;           // カラーバンド数 (2 ~ 16, default: 5)
    float saturationBoost; // 彩度ブースト (0.0 ~ 2.0, default: 1.2)
    float2 padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// RGB → HSV
float3 RGBtoHSV(float3 rgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV → RGB
float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 originalColor = gTexture.Sample(gSampler, input.texCoord);
    float3 c = originalColor.rgb;

    // ポスタリゼーション: 各チャンネルをstepsレベルに量子化
    float fSteps = float(max(steps, 2));
    c = floor(c * fSteps + 0.5) / fSteps;

    // 彩度ブースト (HSVで調整)
    float3 hsv = RGBtoHSV(c);
    hsv.y = saturate(hsv.y * saturationBoost);
    c = HSVtoRGB(hsv);

    output.color = float4(saturate(c), originalColor.a);

    return output;
}

#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ---- スプリットトーニング + バイブランス + 色温度 ----
cbuffer ColorGradeParams : register(b0)
{
    // スプリットトーニング
    float3 shadowColor;     // 影の色オフセット (default: 0, 0, 0)
    float  splitBalance;    // 影/ハイライトの境界輝度 (0.0~1.0, default: 0.45)

    float3 highlightColor;  // ハイライトの色オフセット (default: 0, 0, 0)
    float  splitStrength;   // スプリットの強さ (0.0~1.0, default: 0.25)

    // バイブランス・色温度
    float  vibrance;        // バイブランス (-1.0~1.0, default: 0.3)
    float  colorTemp;       // 色温度 (-1.0寒色 ~ +1.0暖色, default: 0.08)
    float  colorTint;       // ティント (-1.0緑 ~ +1.0紫, default: 0.0)
    float  padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

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

    float4 original = gTexture.Sample(gSampler, input.texCoord);
    float3 c = original.rgb;

    // ===== 1. スプリットトーニング =====
    // 輝度に応じて影/ハイライトの影響度を計算
    float lum = Luminance(c);
    float shadowW    = 1.0 - smoothstep(0.0, splitBalance + 0.25, lum);
    float highlightW = smoothstep(splitBalance - 0.25, 1.0, lum);

    c += shadowColor    * shadowW    * splitStrength;
    c += highlightColor * highlightW * splitStrength;

    // ===== 2. バイブランス (彩度が低いピクセルほど強く) =====
    {
        float3 hsv = RGBtoHSV(c);
        // 既に高彩度のものには効きにくい (肌色保護)
        float vibranceBoost = (1.0 - hsv.y) * vibrance * 0.6;
        hsv.y = saturate(hsv.y + vibranceBoost);
        c = HSVtoRGB(hsv);
    }

    // ===== 3. 色温度 (赤-青軸) + ティント (緑-紫軸) =====
    // 暖色: 赤↑ 青↓ / 寒色: 赤↓ 青↑
    c.r = saturate(c.r + colorTemp * 0.12);
    c.g = saturate(c.g + colorTemp * 0.03);
    c.b = saturate(c.b - colorTemp * 0.15);
    // ティント: 緑↑ → マゼンタ↓ / 紫↑ → 緑↓
    c.g = saturate(c.g - colorTint * 0.10);
    c.r = saturate(c.r + colorTint * 0.05);
    c.b = saturate(c.b + colorTint * 0.05);

    output.color = float4(saturate(c), original.a);

    return output;
}

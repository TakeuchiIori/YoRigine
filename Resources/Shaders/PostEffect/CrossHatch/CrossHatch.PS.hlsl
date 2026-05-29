#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer CrossHatchParams : register(b0)
{
    float lineSpacing;  // ハッチング線の間隔 [pixels] (4.0 ~ 16.0, default: 8.0)
    float lineWidth;    // 線の太さ (0.5 ~ 3.0, default: 1.0)
    float strength;     // 強度 (0.0 ~ 1.0, default: 0.7)
    float padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// 指定角度のハッチング線パターン (0=線なし, 1=線あり)
float HatchLine(float2 pixel, float angle, float spacing, float width)
{
    float cosA = cos(angle);
    float sinA = sin(angle);
    float proj = cosA * pixel.x + sinA * pixel.y;
    float t = frac(proj / spacing);
    float lineHalf = (width * 0.5) / spacing;
    return step(t, lineHalf) + step(1.0 - lineHalf, t);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 pixel = input.texCoord * float2(width, height);

    float4 original = gTexture.Sample(gSampler, input.texCoord);
    float lum = Luminance(original.rgb);

    // 輝度に応じて最大4レイヤーのハッチングを重ねる
    // 暗い部分ほど多くのハッチングレイヤーが加わる
    static const float PI = 3.14159265;

    float hatch = 0.0;

    // Layer 1: 45度 (lum < 0.75 の領域)
    if (lum < 0.75)
    {
        hatch = max(hatch, HatchLine(pixel, PI * 0.25, lineSpacing, lineWidth));
    }
    // Layer 2: 135度 (lum < 0.55)
    if (lum < 0.55)
    {
        hatch = max(hatch, HatchLine(pixel, PI * 0.75, lineSpacing, lineWidth));
    }
    // Layer 3: 水平 (lum < 0.35)
    if (lum < 0.35)
    {
        hatch = max(hatch, HatchLine(pixel, 0.0, lineSpacing, lineWidth));
    }
    // Layer 4: 垂直 (lum < 0.15 — 最も暗い部分)
    if (lum < 0.15)
    {
        hatch = max(hatch, HatchLine(pixel, PI * 0.5, lineSpacing, lineWidth));
    }

    // ハッチング線の色は暗めの線（元の色を強調）
    float3 hatchColor = original.rgb * 0.15;

    float3 result = lerp(original.rgb, hatchColor, hatch * strength);

    output.color = float4(result, original.a);

    return output;
}

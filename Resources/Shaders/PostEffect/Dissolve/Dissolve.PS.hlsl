#include "../FullScreen./FullScreen.hlsli"

cbuffer DissolveParams : register(b0)
{
    float gThreshold;       // 閾値
    float gEdgeWidth;       // エッジ幅
    float3 gEdgeColor;      // エッジ色
    bool gInvert;           // true で反転
};

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float mask = gMaskTexture.Sample(gSampler, input.texCoord);

    // 反転対応
    float dissolveFactor = gInvert ? (1.0f - gThreshold) : gThreshold;

    // 輪郭判定
    float edge = smoothstep(dissolveFactor, dissolveFactor + gEdgeWidth, mask);

    // 描画／破棄
    if (mask < dissolveFactor)
    {
        discard;
    }

    float4 baseColor = gTexture.Sample(gSampler, input.texCoord);
    float3 finalColor = lerp(gEdgeColor, baseColor.rgb, edge);

    output.color = float4(finalColor, baseColor.a);
    return output;
}

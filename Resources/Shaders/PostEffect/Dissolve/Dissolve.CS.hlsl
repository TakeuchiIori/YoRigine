#include "../Common/PostEffectCS.hlsli"

cbuffer DissolveParams : register(b0)
{
    float gThreshold;
    float gEdgeWidth;
    float3 gEdgeColor;
    bool gInvert;
};

Texture2D<float4> gInput : register(t0);
Texture2D<float> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float mask = gMaskTexture.SampleLevel(gSampler, uv, 0);

    float dissolveFactor = gInvert ? (1.0f - gThreshold) : gThreshold;
    float edge = smoothstep(dissolveFactor, dissolveFactor + gEdgeWidth, mask);

    if (mask < dissolveFactor)
    {
        // PS版の discard 相当: 出力を完全に黒透明にする
        gOutput[dtid.xy] = float4(0, 0, 0, 0);
        return;
    }

    float4 baseColor = gInput.SampleLevel(gSampler, uv, 0);
    float3 finalColor = lerp(gEdgeColor, baseColor.rgb, edge);
    finalColor = LinearToSRGB(finalColor);
    gOutput[dtid.xy] = float4(finalColor, baseColor.a);
}

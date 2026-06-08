#include "../Common/PostEffectCS.hlsli"

cbuffer ExposureBuffer : register(b0)
{
    float exposure;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float3 ToneMapACES(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float3 hdr = gInput.SampleLevel(gSampler, uv, 0).rgb;
    hdr *= exposure;

    float3 mapped = ToneMapACES(hdr);
    // PS版と同じ二重ガンマを再現するため、 pow(1/2.2) + LinearToSRGB の両方を適用
    mapped = pow(mapped, 1.0f / 2.2f);
    mapped = LinearToSRGB(mapped);
    gOutput[dtid.xy] = float4(mapped, 1.0f);
}

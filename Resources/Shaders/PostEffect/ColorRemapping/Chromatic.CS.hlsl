#include "../Common/PostEffectCS.hlsli"

cbuffer ChromaticParams : register(b0)
{
    float aberrationStrength;
    float2 screenSize;
    float edgeStrength;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float2 center = float2(0.5f, 0.5f);
    float2 dir = uv - center;
    float dist = length(dir);

    float falloff = smoothstep(0.0f, 0.7f, dist);
    float aberration = aberrationStrength * falloff;
    float2 uvOffset = normalize(dir) * aberration;

    float r = gInput.SampleLevel(gSampler, uv - uvOffset, 0).r;
    float g = gInput.SampleLevel(gSampler, uv, 0).g;
    float b = gInput.SampleLevel(gSampler, uv + uvOffset, 0).b;

    float3 c = float3(r, g, b);
    c = LinearToSRGB(c);
    gOutput[dtid.xy] = float4(c, 1.0f);
}

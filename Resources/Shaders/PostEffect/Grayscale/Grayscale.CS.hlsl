#include "../Common/PostEffectCS.hlsli"

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
    float4 c = gInput.SampleLevel(gSampler, uv, 0);

    float value = dot(c.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    c.rgb = float3(value, value, value);

    c.rgb = LinearToSRGB(c.rgb);
    gOutput[dtid.xy] = c;
}

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

    float2 correct = uv * (1.0f - uv.yx);
    float vignette = correct.x * correct.y * 16.0f;
    vignette = saturate(pow(vignette, 0.8f));

    c.rgb *= vignette;
    c.rgb = LinearToSRGB(c.rgb);
    gOutput[dtid.xy] = c;
}

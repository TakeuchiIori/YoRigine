#include "../Common/PostEffectCS.hlsli"

cbuffer KernelSettings : register(b0)
{
    int kernelSize;
    float sigma;
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

    float2 uvStep = float2(rcp(float(w)), rcp(float(h)));
    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    int r = kernelSize / 2;
    int n = 2 * r + 1;
    float invCount = rcp(float(n * n));

    float3 sum = float3(0, 0, 0);
    [loop]
    for (int y = -r; y <= r; ++y)
    {
        [loop]
        for (int x = -r; x <= r; ++x)
        {
            sum += gInput.SampleLevel(gSampler, uv + float2(x, y) * uvStep, 0).rgb;
        }
    }
    sum *= invCount;

    sum = LinearToSRGB(sum);
    gOutput[dtid.xy] = float4(sum, 1.0f);
}

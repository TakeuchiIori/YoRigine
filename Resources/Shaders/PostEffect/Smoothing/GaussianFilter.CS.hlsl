#include "../Common/PostEffectCS.hlsli"

cbuffer KernelSettings : register(b0)
{
    int kernelSize;
    float sigma;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

static const float PI = 3.14159265f;

float gauss(float x, float y, float s)
{
    float exponent = -(x * x + y * y) / (2.0f * s * s);
    return exp(exponent) / (2.0f * PI * s * s);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uvStep = float2(rcp(float(w)), rcp(float(h)));
    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float3 sum = float3(0, 0, 0);
    float weightSum = 0.0f;

    int r = kernelSize / 2;
    [loop]
    for (int y = -r; y <= r; ++y)
    {
        [loop]
        for (int x = -r; x <= r; ++x)
        {
            float wgt = gauss((float) x, (float) y, sigma);
            float2 sampleUV = uv + float2(x, y) * uvStep;
            sum += gInput.SampleLevel(gSampler, sampleUV, 0).rgb * wgt;
            weightSum += wgt;
        }
    }

    float3 result = sum / max(weightSum, 1e-5f);
    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, 1.0f);
}

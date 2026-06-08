#include "../Common/PostEffectCS.hlsli"

cbuffer KuwaharaParams : register(b0)
{
    int radius;
    float sharpness;
    float2 padding;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

void ComputeQuadrant(
    float2 uv, float2 texelSize,
    int x0, int x1, int y0, int y1,
    out float3 outMean, out float outVariance)
{
    float3 sum = float3(0, 0, 0);
    float3 sumSq = float3(0, 0, 0);
    float count = 0.0f;

    [loop]
    for (int x = x0; x <= x1; ++x)
    {
        [loop]
        for (int y = y0; y <= y1; ++y)
        {
            float3 c = gInput.SampleLevel(gSampler, uv + float2(x, y) * texelSize, 0).rgb;
            sum += c;
            sumSq += c * c;
            count += 1.0f;
        }
    }

    outMean = sum / count;
    float3 variance3 = sumSq / count - outMean * outMean;
    outVariance = Luminance(max(variance3, 0.0f));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float2 texelSize = float2(rcp(float(w)), rcp(float(h)));
    int r = clamp(radius, 1, 8);

    float3 mean[4];
    float variance[4];

    ComputeQuadrant(uv, texelSize, -r,  0, -r,  0, mean[0], variance[0]);
    ComputeQuadrant(uv, texelSize,  0,  r, -r,  0, mean[1], variance[1]);
    ComputeQuadrant(uv, texelSize, -r,  0,  0,  r, mean[2], variance[2]);
    ComputeQuadrant(uv, texelSize,  0,  r,  0,  r, mean[3], variance[3]);

    float3 result = float3(0, 0, 0);
    float totalWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float wgt = 1.0f / (variance[i] * sharpness + 1e-5f);
        result += mean[i] * wgt;
        totalWeight += wgt;
    }

    result /= totalWeight;
    float4 original = gInput.SampleLevel(gSampler, uv, 0);

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, original.a);
}

#include "../Common/PostEffectCS.hlsli"

struct BlurParams
{
    float2 blurDirection;
    float2 blurCenter;
    float blurWidth;
    int sampleCount;
    bool isRadial;
    float padding[1];
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<BlurParams> gBlurParams : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float2 direction = gBlurParams.isRadial
        ? (uv - gBlurParams.blurCenter)
        : gBlurParams.blurDirection;
    direction = normalize(direction);

    float3 sum = float3(0, 0, 0);
    int n = gBlurParams.sampleCount;
    [loop]
    for (int i = 0; i <= n; ++i)
    {
        float2 offset = direction * (gBlurParams.blurWidth * i);
        sum += gInput.SampleLevel(gSampler, uv + offset, 0).rgb;
    }
    sum *= rcp((float) (n + 1));

    sum = LinearToSRGB(sum);
    gOutput[dtid.xy] = float4(sum, 1.0f);
}

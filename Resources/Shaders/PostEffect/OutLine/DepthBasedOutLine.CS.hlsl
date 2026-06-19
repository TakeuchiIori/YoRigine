#include "../Common/PostEffectCS.hlsli"

struct Material
{
    float4x4 projectionInverse;
    int kernelSize;
    float4 outlineColor;
};

Texture2D<float4> gInput : register(t0);
Texture2D<float>  gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<Material> gMaterial : register(b0);

float SampleViewZ(float2 uv)
{
    float ndcDepth = gDepthTexture.SampleLevel(gSamplerPoint, uv, 0);
    float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
    return viewSpace.z * rcp(viewSpace.w);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uvStep = float2(rcp(float(w)), rcp(float(h)));
    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    // Prewitt 微分カーネル (任意kernelSizeに一般化)
    int r = gMaterial.kernelSize / 2;
    int n = 2 * r + 1;
    float invH = rcp(float(n * n - n)); // 水平方向はn*r個ずつ符号化される

    float2 diff = float2(0, 0);
    [loop]
    for (int y = -r; y <= r; ++y)
    {
        [loop]
        for (int x = -r; x <= r; ++x)
        {
            float viewZ = SampleViewZ(uv + float2(x, y) * uvStep);
            float hSign = (x > 0) ? 1.0f : (x < 0 ? -1.0f : 0.0f);
            float vSign = (y > 0) ? 1.0f : (y < 0 ? -1.0f : 0.0f);
            diff.x += viewZ * hSign * invH;
            diff.y += viewZ * vSign * invH;
        }
    }

    float weight = saturate(length(diff));
    float3 sceneColor = gInput.SampleLevel(gSampler, uv, 0).rgb;
    float3 result = lerp(gMaterial.outlineColor.rgb, sceneColor, 1.0f - weight);

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, 1.0f);
}

#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer KuwaharaParams : register(b0)
{
    int   radius;      // フィルター半径 (1 ~ 8, default: 4)
    float sharpness;   // エッジのシャープさ (1.0 ~ 8.0, default: 4.0)
    float2 padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// 1つの象限の平均色と輝度分散を計算
void ComputeQuadrant(
    float2 uv, float2 texelSize,
    int x0, int x1, int y0, int y1,
    out float3 outMean, out float outVariance)
{
    float3 sum = float3(0, 0, 0);
    float3 sumSq = float3(0, 0, 0);
    float count = 0;

    [loop]
    for (int x = x0; x <= x1; ++x)
    {
        [loop]
        for (int y = y0; y <= y1; ++y)
        {
            float3 c = gTexture.Sample(gSampler, uv + float2(x, y) * texelSize).rgb;
            sum   += c;
            sumSq += c * c;
            count += 1.0;
        }
    }

    outMean = sum / count;
    float3 variance3 = sumSq / count - outMean * outMean;
    // スカラー分散として輝度を使う
    outVariance = Luminance(max(variance3, 0.0));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = float2(rcp(float(width)), rcp(float(height)));

    int r = clamp(radius, 1, 8);

    // 4象限の平均と分散
    float3 mean[4];
    float  variance[4];

    ComputeQuadrant(input.texCoord, texelSize, -r,  0, -r,  0, mean[0], variance[0]); // NW
    ComputeQuadrant(input.texCoord, texelSize,  0,  r, -r,  0, mean[1], variance[1]); // NE
    ComputeQuadrant(input.texCoord, texelSize, -r,  0,  0,  r, mean[2], variance[2]); // SW
    ComputeQuadrant(input.texCoord, texelSize,  0,  r,  0,  r, mean[3], variance[3]); // SE

    // 分散最小の象限の平均色を選ぶ（sharpnessで重み付け平均）
    float3 result = float3(0, 0, 0);
    float totalWeight = 0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        // 低分散ほど高いウェイト
        float w = 1.0 / (variance[i] * sharpness + 1e-5);
        result += mean[i] * w;
        totalWeight += w;
    }

    result /= totalWeight;

    float4 original = gTexture.Sample(gSampler, input.texCoord);
    output.color = float4(result, original.a);

    return output;
}

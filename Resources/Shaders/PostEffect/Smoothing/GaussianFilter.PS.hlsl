#include "../FullScreen./FullScreen.hlsli"
static const float PI = 3.14159265f;
float gauss(float x, float y, float sigma)
{
    float exponent = -(x * x + y * y) / rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) / rcp(denominator);
}
static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
};
static const float kKernel3x3[3][3] =
{
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
};
static const float2 kIndex5x5[5][5] =
{
    { { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f } },
    { { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f } },
    { { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f } },
    { { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f } },
    { { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f } },
};
static const float kKernel5x5[5][5] =
{
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
};
static const float2 kIndex7x7[7][7] =
{
    { { -3.0f, -3.0f }, { -2.0f, -3.0f }, { -1.0f, -3.0f }, { 0.0f, -3.0f }, { 1.0f, -3.0f }, { 2.0f, -3.0f }, { 3.0f, -3.0f } },
    { { -3.0f, -2.0f }, { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f }, { 3.0f, -2.0f } },
    { { -3.0f, -1.0f }, { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f }, { 3.0f, -1.0f } },
    { { -3.0f, 0.0f }, { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f } },
    { { -3.0f, 1.0f }, { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f }, { 3.0f, 1.0f } },
    { { -3.0f, 2.0f }, { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 2.0f } },
    { { -3.0f, 3.0f }, { -2.0f, 3.0f }, { -1.0f, 3.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 3.0f } },
};
static const float kKernel7x7[7][7] =
{
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
};
static const float2 kIndex9x9[9][9] =
{
    { { -4.0f, -4.0f }, { -3.0f, -4.0f }, { -2.0f, -4.0f }, { -1.0f, -4.0f }, { 0.0f, -4.0f }, { 1.0f, -4.0f }, { 2.0f, -4.0f }, { 3.0f, -4.0f }, { 4.0f, -4.0f } },
    { { -4.0f, -3.0f }, { -3.0f, -3.0f }, { -2.0f, -3.0f }, { -1.0f, -3.0f }, { 0.0f, -3.0f }, { 1.0f, -3.0f }, { 2.0f, -3.0f }, { 3.0f, -3.0f }, { 4.0f, -3.0f } },
    { { -4.0f, -2.0f }, { -3.0f, -2.0f }, { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f }, { 3.0f, -2.0f }, { 4.0f, -2.0f } },
    { { -4.0f, -1.0f }, { -3.0f, -1.0f }, { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f }, { 3.0f, -1.0f }, { 4.0f, -1.0f } },
    { { -4.0f, 0.0f }, { -3.0f, 0.0f }, { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f }, { 4.0f, 0.0f } },
    { { -4.0f, 1.0f }, { -3.0f, 1.0f }, { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f }, { 3.0f, 1.0f }, { 4.0f, 1.0f } },
    { { -4.0f, 2.0f }, { -3.0f, 2.0f }, { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 2.0f }, { 4.0f, 2.0f } },
    { { -4.0f, 3.0f }, { -3.0f, 3.0f }, { -2.0f, 3.0f }, { -1.0f, 3.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 3.0f }, { 4.0f, 3.0f } },
    { { -4.0f, 4.0f }, { -3.0f, 4.0f }, { -2.0f, 4.0f }, { -1.0f, 4.0f }, { 0.0f, 4.0f }, { 1.0f, 4.0f }, { 2.0f, 4.0f }, { 3.0f, 4.0f }, { 4.0f, 4.0f } },
};
static const float kKernel9x9[9][9] =
{
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
};
cbuffer KernelSettings : register(b0)
{
    int kernelSize;
    float sigma;
};
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};
PixelShaderOutput main(VertexShaderOutput input)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(rcp(float(width)), rcp(float(height)));
    
    PixelShaderOutput output;
    output.color.rgb = float3(0.0f, 0.0f, 0.0f);
    output.color.a = 1.0f;
    float weightSum = 0.0f;
    float kernel3x3[3][3];
    float kernel5x5[5][5];
    float kernel7x7[7][7];
    float kernel9x9[9][9];
    
    if (kernelSize == 3)
    {
        for (int x = 0; x < 3; ++x)
        {
            for (int y = 0; y < 3; ++y)
            {
                kernel3x3[x][y] = gauss(kIndex3x3[x][y].x, kIndex3x3[x][y].y, sigma);
                weightSum += kernel3x3[x][y];
            }
        }
        
        for (int a = 0; a < 3; ++a)
        {
            for (int b = 0; b < 3; ++b)
            {
                float2 texcoord = input.texCoord + kIndex3x3[a][b] * uvStepSize;
                float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
                output.color.rgb += fetchColor * kKernel3x3[a][b];
            }
        }
    }
    else if (kernelSize == 5)
    {
        for (int x = 0; x < 5; ++x)
        {
            for (int y = 0; y < 5; ++y)
            {
                kernel5x5[x][y] = gauss(kIndex5x5[x][y].x, kIndex5x5[x][y].y, sigma);
                weightSum += kernel5x5[x][y];
            }
        }
        
        for (int a = 0; a < 5; ++a)
        {
            for (int b = 0; b < 5; ++b)
            {
                float2 texcoord = input.texCoord + kIndex5x5[a][b] * uvStepSize;
                float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
                output.color.rgb += fetchColor * kKernel5x5[a][b];
            }
        }
    }
    else if (kernelSize == 7)
    {
        for (int x = 0; x < 7; ++x)
        {
            for (int y = 0; y < 7; ++y)
            {
                kernel7x7[x][y] = gauss(kIndex7x7[x][y].x, kIndex7x7[x][y].y, sigma);
                weightSum += kernel7x7[x][y];
            }
        }
        
        for (int a = 0; a < 7; ++a)
        {
            for (int b = 0; b < 7; ++b)
            {
                float2 texcoord = input.texCoord + kIndex7x7[a][b] * uvStepSize;
                float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
                output.color.rgb += fetchColor * kKernel7x7[a][b];
            }
        }
    }
    else if (kernelSize == 9)
    {
        for (int x = 0; x < 9; ++x)
        {
            for (int y = 0; y < 9; ++y)
            {
                kernel9x9[x][y] = gauss(kIndex9x9[x][y].x, kIndex9x9[x][y].y, sigma);
                weightSum += kernel9x9[x][y];
            }
        }
        
        for (int a = 0; a < 9; ++a)
        {
            for (int b = 0; b < 9; ++b)
            {
                float2 texcoord = input.texCoord + kIndex9x9[a][b] * uvStepSize;
                float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
                output.color.rgb += fetchColor * kKernel9x9[a][b];
            }
        }
    }
    
    return output;
}
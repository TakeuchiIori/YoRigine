#include "../FullScreen./FullScreen.hlsli"


struct BlurParams
{
    float2 blurDirection; // 横:縦
    float2 blurCenter; // ブラーの中心
    float blurWidth; // サンプリング間隔
    int sampleCount; // サンプル数
    bool isRadial; // false: 方向ブラー, true: ラジアルブラー
    float padding[1]; // 16BYTEに合わせる
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<BlurParams> gBlurParams : register(b0);


PixelShaderOutput main(VertexShaderOutput input)
{
    float2 direction;
    
    
    if (gBlurParams.isRadial)
    {
        direction = input.texCoord - gBlurParams.blurCenter;
    }
    else
    {
        direction = gBlurParams.blurDirection;
    }
    
    direction = normalize(direction);
    
    
    float3 outputColor = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i <= gBlurParams.sampleCount; i++)
    {
        float2 offset = direction * (gBlurParams.blurWidth * i);
        float2 samplesTexCoord = input.texCoord + offset;
        outputColor += gTexture.Sample(gSampler, samplesTexCoord).rgb;
    }

    outputColor.rgb *= rcp((float) (gBlurParams.sampleCount + 1));

    
    PixelShaderOutput output;
    output.color.rgb = outputColor;
    output.color.a = 1.0f;
    
    return output;
    
}
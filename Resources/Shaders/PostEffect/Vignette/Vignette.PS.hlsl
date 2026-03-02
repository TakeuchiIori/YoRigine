#include "../FullScreen./FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    // 中心になるほど明るくなるように計算する
    output.color = gTexture.Sample(gSampler, input.texCoord);

    float2 correct = input.texCoord * (1.0f - input.texCoord.yx);
    float vignette = correct.x * correct.y * 16.0f;
    
    vignette = saturate(pow(vignette, 0.8f)); // 右値が大きいほど暗くなる
    
    output.color.rgb *= vignette;
    return output;
}
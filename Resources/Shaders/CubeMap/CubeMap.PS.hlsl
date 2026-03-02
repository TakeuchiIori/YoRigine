#include "CubeMap.hlsli"


cbuffer gMaterialColor : register(b0)
{
    float4 color : SV_TARGET0;
};


TextureCube<float4> gTexture: register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
   
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * color;
    return output;
}
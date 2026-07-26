#include "GPUParticle.hlsli"
struct MaterialColor
{
    float4 color : SV_TARGET0;
};

struct MaterialUV
{
    float4x4 uvTransform;
};


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<MaterialColor> gMaterialColor : register(b1);
ConstantBuffer<MaterialUV> gMaterialUV : register(b2);

Texture2D<float4> gTexture : register(t1);
SamplerState gSampler : register(s0);
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialUV.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    output.color = gMaterialColor.color * textureColor * input.color;
    if (output.color.a < 0.01f)
    {
        discard;
    }
    
    return output;
}

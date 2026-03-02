#include "Shadowmap.hlsli"

ConstantBuffer<ObjectTransform> gObject : register(b0);
ConstantBuffer<LightMatrices> gLight : register(b1);

struct VertexShaderInput
{
    float3 position : POSITION;
};


VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 wpos = mul(float4(input.position, 1.0f), gObject.world);
    output.position = mul(wpos, gLight.lightViewProjection);
    
    return output;
}
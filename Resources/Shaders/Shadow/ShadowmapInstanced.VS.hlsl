#include "Shadowmap.hlsli"

// Per-instance データ (Object3dInstanced のサブセット: World 行列だけあればOK)
struct InstanceData
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4   color;
    float4x4 uvTransform;
    float    stochasticStrength;
    float    _pad0;
    float    _pad1;
    float    _pad2;
};

StructuredBuffer<InstanceData> gInstances : register(t0);
ConstantBuffer<LightMatrices>  gLight : register(b1);

struct VertexShaderInput
{
    float3 position : POSITION;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    float4 wpos = mul(float4(input.position, 1.0f), gInstances[instanceID].World);
    output.position = mul(wpos, gLight.lightViewProjection);
    return output;
}

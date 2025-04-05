#include "Object3d.hlsli"
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct TransformationMatrixInstances
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

//ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

StructuredBuffer<TransformationMatrixInstances> gTransformationMatrixInstances : register(t1);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};
VertexShaderOutput main(VertexShaderInput input,uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrixInstances[instanceID].WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrixInstances[instanceID].World));
    output.worldPosition = mul(input.position, gTransformationMatrixInstances[instanceID].World).xyz;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrixInstances[instanceID].WorldInverseTranspose));
    return output;
}

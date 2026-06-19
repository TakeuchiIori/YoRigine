#include "Object3dInstanced.hlsli"

// Per-instance データ (CPU側 InstancedObject3d::InstanceData と完全一致させること)
struct InstanceData
{
    float4x4 WVP;                  // 64
    float4x4 World;                // 64
    float4x4 WorldInverseTranspose;// 64
    float4   color;                // 16
    float4x4 uvTransform;          // 64
    float    stochasticStrength;   //  4
    float    _pad0;                //  4
    float    _pad1;                //  4
    float    _pad2;                //  4
};

StructuredBuffer<InstanceData> gInstances : register(t3);

ConstantBuffer<LightMatrices> gLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

InstancedVertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    InstancedVertexShaderOutput output;

    InstanceData inst = gInstances[instanceID];

    float4 worldPosH = mul(input.position, inst.World);
    output.worldPosition = worldPosH.xyz;
    output.position = mul(worldPosH, gCamera.viewProjection);
    output.normal = normalize(mul(input.normal, (float3x3) inst.WorldInverseTranspose));
    output.texcoord = input.texcoord;
    output.shadowPos = mul(worldPosH, gLight.lightViewProjection);
    output.instanceID = instanceID;
    return output;
}

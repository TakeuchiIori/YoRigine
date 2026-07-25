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
    float    outlineMask;          //  4  (輪郭線の掛け率。本体描画では未使用)
    float    ditherFade;           //  4  (1=alpha をディザー透過率に使用)
    float    _pad2;                //  4
};

StructuredBuffer<InstanceData> gInstances : register(t3);

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
    // シャドウのカスケード選択・サンプルは PS 側で worldPosition から行う
    output.instanceID = instanceID;
    return output;
}

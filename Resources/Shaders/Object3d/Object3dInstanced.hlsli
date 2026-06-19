#ifndef OBJECT3D_INSTANCED_HLSLI
#define OBJECT3D_INSTANCED_HLSLI

#include "Object3d.hlsli"

// Instanced 用に instanceID を VS→PS で受け渡すための拡張出力構造
// SV_InstanceID は PS の直接入力では reflection が混乱する場合があるので
// nointerpolation 付きの自前セマンティクスで持ち回す。
struct InstancedVertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 shadowPos : TEXCOORD1;
    nointerpolation uint instanceID : INSTANCEID;
};

#endif

#include "Skinning.hlsli"
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Skinned
{
    float4 position;
    float3 normal;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    
    // スキニングの計算：位置変換
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;

    skinned.position.w = 1.0f; // 均質座標

    // スキニングの計算：法線変換
    skinned.normal = mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal); // 正規化して戻す
    
    
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input); // まずSkinning計算を行って、Skinning後の頂点情報を手に入れる。ここでの頂点もSkeletonSpace
    // Skinning結果を使って計算
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
   // output.normal = normalize(mul(skinned.normal, (float3x3) gTransformationMatrix.World));
    output.texcoord = input.texcoord;
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    output.normal = normalize(mul(skinned.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    return output;
}


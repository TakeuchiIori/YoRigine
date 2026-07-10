#include "Object3d.hlsli"
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<Camera> gCamera : register(b3);
struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // ワールド座標の計算
    float4 worldPosH = mul(input.position, gTransformationMatrix.World);
    output.worldPosition = worldPosH.xyz;

    // 最終的なクリップ座標の計算
    output.position = mul(worldPosH,gCamera.viewProjection);
    // 法線ベクトルの変換
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    // その他の情報を出力
    output.texcoord = input.texcoord;
    // シャドウのカスケード選択・サンプルは PS 側で worldPosition から行う
    return output;
}

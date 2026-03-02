#include "Effect.hlsli"

// ==========================================
//  定数バッファ定義
// ==========================================
struct TransformationMatrix
{
    float4x4 World; // ワールド行列
    float4x4 WorldInverseTranspose; // 法線変換用
};

struct Instansing
{
    TransformationMatrix transformation;
    float4 color;
};
//　影計算用のライト行列 (ShadowMap用)
struct LightMatrices
{
    float4x4 lightViewProjection;
};

// カメラ行列
struct Camera
{
    float3 worldPosition;
    float4x4 viewProjection;
};

ConstantBuffer<LightMatrices> gLight : register(b0);
ConstantBuffer<Camera> gCamera : register(b8);

StructuredBuffer<Instansing> gInstancingData : register(t0);

// ==========================================
//  メイン関数
// ==========================================
VertexShaderOutput main(VertexShaderInput input,uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    TransformationMatrix instData = gInstancingData[instanceID].transformation;
    
    // ワールド座標の計算 (ローカル座標 * ワールド行列)
    float4 worldPosH = mul(input.position, instData.World);
    output.worldPosition = worldPosH.xyz;
    
    // 画面上の座標 (ワールド座標 * カメラのViewProj)
    output.position = mul(worldPosH, gCamera.viewProjection);

    // 法線ベクトルの変換 (回転のみ適用)
    output.normal = normalize(mul(input.normal, (float3x3) instData.WorldInverseTranspose));

    // UV座標はそのままパス
    output.texcoord = input.texcoord;

    // シャドウマップ用座標 (ワールド座標 * ライトの行列)
    output.shadowPos = mul(worldPosH, gLight.lightViewProjection);
    
    // インスタンスごとの色を出力
    output.color = gInstancingData[instanceID].color;
    return output;
}
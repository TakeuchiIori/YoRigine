#include "YParticle.hlsli"

// インスタンシング用のGPUデータ構造体
struct ParticleInstance
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

// StructuredBuffer（インスタンシングデータ）
StructuredBuffer<ParticleInstance> gParticle : register(t0);

// 頂点シェーダーの入力
struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// 頂点シェーダーのメイン関数
VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // インスタンスごとのWVP行列で座標変換
    output.position = mul(input.position, gParticle[instanceId].WVP);
    
    // テクスチャ座標をそのまま渡す
    output.texcoord = input.texcoord;
    
    // 法線をワールド空間に変換
    output.normal = normalize(mul(input.normal, (float3x3) gParticle[instanceId].World));
    
    // インスタンスごとのカラーを渡す
    output.color = gParticle[instanceId].color;
    
    // ワールド座標を計算して渡す
    output.worldPosition = mul(input.position, gParticle[instanceId].World).xyz;
    
    return output;
}

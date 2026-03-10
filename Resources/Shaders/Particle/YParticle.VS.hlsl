#include "YParticle.hlsli"

// インスタンシング用のGPUデータ構造体
struct ParticleInstance
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4x4 uvTransform;
};

// StructuredBuffer（インスタンシングデータ）
StructuredBuffer<ParticleInstance> gParticle : register(t0);

cbuffer InstanceOffsetCB : register(b0) // 既存のbスロットと被らない番号に
{
    uint gInstanceOffset;
    float3 padding;
}

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
    uint id = instanceId + gInstanceOffset; // ★ここだけ変更
    
    // インスタンスごとのWVP行列で座標変換
    output.position = mul(input.position, gParticle[id].WVP);
    
    // テクスチャ座標をインスタンスごとのUV変換行列で変換
    output.texcoord = mul(float4(input.texcoord, 0.0f, 1.0f), gParticle[id].uvTransform).xy;
    
    // 法線をワールド空間に変換
    output.normal = normalize(mul(input.normal, (float3x3) gParticle[id].World));
    
    // インスタンスごとのカラーを渡す
    output.color = gParticle[id].color;
    
    // ワールド座標を計算して渡す
    output.worldPosition = mul(input.position, gParticle[id].World).xyz;
    
    return output;
}

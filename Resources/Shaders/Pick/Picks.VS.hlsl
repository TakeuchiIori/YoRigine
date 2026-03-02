// PickVS.hlsl
// GPU Pick Buffer 用 Vertex Shader
//
// 既存 Mesh の頂点バッファを流用するため、
// 頂点レイアウトは通常の Object3d と同じ形式にしている。
//   float4 POSITION  (offset  0, 16byte)
//   float3 NORMAL    (offset 16, 12byte)
//   float2 TEXCOORD  (offset 28,  8byte)
//   stride = 36byte
//
// NORMAL / TEXCOORD は変換に使わないが、
// InputLayout の stride を合わせるために宣言している。

struct VSIn
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

// WorldViewProjection 行列（ObjectSelector から WorldTransform の CBV を渡す）
cbuffer MVP : register(b0)
{
    float4x4 gWVP;
};

float4 main(VSIn v) : SV_POSITION
{
    // w = 1.0 で渡ってくるが一応 v.position.w そのままで mul する
    return mul(v.position, gWVP);
}


struct VertexShaderOutput{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

// ライトのViewProjection（影パス VS 用）
struct LightMatrices
{
    float4x4 lightViewProjection;
};
struct Camera
{
    float3 worldPosition;
    float4x4 viewProjection;
};


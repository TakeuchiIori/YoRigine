struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

// オブジェクト（world行列）
struct ObjectTransform
{
    float4x4 world;
};

// ライトのViewProjection
struct LightMatrices
{
    float4x4 lightViewProjection;
};
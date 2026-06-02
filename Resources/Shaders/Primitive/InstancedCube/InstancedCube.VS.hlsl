// ============================================================
// InstancedCube VS
//   - 単位ライン形状(立方体ワイヤーなど)を 1 つのVBに持っておく
//   - 描画ごとに InstanceData(StructuredBuffer) を引いて
//     ワールド変換 + 色を適用する。1 DrawInstanced で N 個描画する。
// ============================================================

struct Transformation
{
    float4x4 VP; // View * Projection 行列
};
ConstantBuffer<Transformation> gTransformationMatrix : register(b0);

struct InstanceData
{
    float4x4 worldMat;
    float4   color;
};
StructuredBuffer<InstanceData> gInstances : register(t0);

struct VSIn
{
    float4 position : POSITION0;
};
struct VSOut
{
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
};

VSOut main(VSIn input, uint instanceID : SV_InstanceID)
{
    VSOut output;
    InstanceData inst = gInstances[instanceID];
    float4 worldPos = mul(input.position, inst.worldMat);
    output.position = mul(worldPos, gTransformationMatrix.VP);
    output.color    = inst.color;
    return output;
}

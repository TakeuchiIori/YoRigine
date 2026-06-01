// ============================================================
// InstancedCube PS
//   - Vertex Shader が運んだ色をそのまま出力
// ============================================================
struct PSIn
{
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
};

struct PSOut
{
    float4 color : SV_TARGET0;
};

PSOut main(PSIn input)
{
    PSOut output;
    output.color = input.color;
    return output;
}

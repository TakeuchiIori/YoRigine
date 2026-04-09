#include "VfxMesh_Common.hlsli"

ConstantBuffer<Camera> gCamera : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // 頂点はワールド空間で生成済み → World行列不要
    float4 worldPos4 = float4(input.position, 1.0f);

    output.position      = mul(worldPos4, gCamera.viewProjection);
    output.worldPosition = input.position;

    // そのまま PS へ転送
    output.texcoord = input.texcoord;
    output.color    = input.color;
    output.age      = input.age;

    return output;
}

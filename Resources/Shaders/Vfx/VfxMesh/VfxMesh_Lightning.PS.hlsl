#include "VfxMesh_Common.hlsli"

// プロシージャル稲妻。リボン断面(texcoord.y)の中心を細く高輝度な芯にし、
// エネルギーラインで内部のチラつきを加える。加算ブレンド前提・HDRでBloom。

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<LightningParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // リボン横断方向 0..1 → 中心(0.5)で 0、端で 1
    float across = abs(input.texcoord.y * 2.0f - 1.0f);

    // 細く鋭い芯（glowPower が大きいほど細い）
    float core = pow(saturate(1.0f - across), 1.0f + gMeshParam.glowPower * 4.0f);

    // 長さ方向に走るエネルギーのチラつき
    float energy = EnergyLines(input.texcoord, gMeshParam.time, 5.0f, 6.0f);

    // 縁のソフトフェード
    float edge = saturate(1.0f - across);

    float intensity = core + energy * edge * 0.5f;
    intensity *= input.color.a;

    if (intensity < 0.004f)
    {
        discard;
    }

    float3 col = gMeshParam.color.rgb * intensity;

    // 加算プリマルチ（CreateAdditive）
    output.color = float4(col, saturate(intensity));
    return output;
}

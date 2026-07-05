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
    float isBranch = step(0.5f, input.color.r);   // 頂点色 r: 0=本線 / 1=枝

    // 細く鋭い芯（glowPower が大きいほど細い）
    float core = pow(saturate(1.0f - across), 1.0f + gMeshParam.glowPower * 4.0f);

    // 実体感: coreWidth 内をべた塗りに近づけ、solidness で透明感を減らす。
    // additive でも芯を太く明るくして「実体」を出す（wispy な透け感を消す）。
    float solid = 1.0f - smoothstep(gMeshParam.coreWidth, gMeshParam.coreWidth + 0.15f, across);
    core = lerp(core, max(core, solid), gMeshParam.solidness);

    // 長さ方向に走るエネルギーのチラつき
    float energy = EnergyLines(input.texcoord, gMeshParam.time, 5.0f, 6.0f);
    float edge   = saturate(1.0f - across);

    float intensity = core + energy * edge * 0.5f;
    intensity *= input.color.a;

    // アウトライン強調: 縁(across が大きい)ほど光らせる。枝を際立たせる用途。
    float outline = smoothstep(0.5f, 0.9f, across) * gMeshParam.outlineIntensity;

    if (intensity + outline < 0.004f)
    {
        discard;
    }

    // 2色: 芯(color) → 縁(glowColor) を across で lerp。枝は branchColor で上書き。
    float3 baseCol = lerp(gMeshParam.color.rgb, gMeshParam.glowColor.rgb, across);
    baseCol = lerp(baseCol, gMeshParam.branchColor.rgb, isBranch);

    // アウトラインの色: 枝なら branchColor、本線なら glowColor
    float3 outlineCol = lerp(gMeshParam.glowColor.rgb, gMeshParam.branchColor.rgb, isBranch);

    float3 col = baseCol * intensity + outlineCol * outline;

    // 加算プリマルチ（CreateAdditive）
    output.color = float4(col, saturate(intensity + outline));
    return output;
}

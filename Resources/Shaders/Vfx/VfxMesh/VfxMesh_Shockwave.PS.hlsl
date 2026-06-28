#include "VfxMesh_Common.hlsli"

// 爆発の衝撃波リング。カメラを向くクワッド上で、中心から外へ広がって
// 消えるリングをアニメする。加算ブレンド・HDR で Bloom が乗る。

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<ShockwaveParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // クワッド中心からの正規化半径（中心0, 円の縁で1）
    float2 c = input.texcoord * 2.0f - 1.0f;
    float  r = length(c);
    if (r > 1.0f) { discard; }

    // 膨張位相: ワンショット時は外部進捗(0→1で1回), 継続時は frac でループ
    float dur   = max(gMeshParam.duration, 0.01f);
    float phase = (gMeshParam.burst >= 0.0f) ? gMeshParam.burst
                                             : frac(gMeshParam.time / dur);

    // リング先端を phase の位置に。thickness で太さ。
    float th = max(gMeshParam.thickness, 0.001f);
    float d  = abs(r - phase);
    float ring = saturate(1.0f - d / th);
    ring = pow(ring, 2.0f); // くっきりさせる

    // 膨張に伴って全体フェード（消えていく）
    float fade = saturate(1.0f - phase);

    // 先端を少し強調（前面の明るい衝撃波面）
    float lead = saturate((r - (phase - th)) / th); // 内→外で増加
    float intensity = ring * fade * (0.6f + 0.6f * lead) * gMeshParam.color.a;

    if (intensity < 0.004f) { discard; }

    float3 col = gMeshParam.color.rgb * intensity;
    output.color = float4(col, saturate(intensity)); // 加算プリマルチ
    return output;
}

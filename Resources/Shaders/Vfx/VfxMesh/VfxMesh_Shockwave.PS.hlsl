#include "VfxMesh_Common.hlsli"

// 爆発の衝撃波リング。カメラを向くクワッド上に「固定半径のリング」を描くだけ。
//   ・膨張は Module(ScaleOverLife) がクワッドを拡大することで表現する
//   ・フェードは Module(FadeInOut / ColorOverLife) が color.a に乗る
// シェーダー側ではアニメを持たない（＝Geometry×Material×Module で量産しやすい）。
// 加算ブレンド・HDR で Bloom が乗る。

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

    // 固定半径 ringRadius にリングを置く。太さ thickness。
    float th = max(gMeshParam.thickness, 0.001f);
    float rr = gMeshParam.ringRadius;
    float d  = abs(r - rr);
    float ring = saturate(1.0f - d / th);
    ring = pow(ring, 2.0f); // くっきりさせる

    // 先端を少し強調（前面の明るい衝撃波面）
    float lead = saturate((r - (rr - th)) / th); // 内→外で増加

    // 不透明度・消え方は Module が color.a に積んでくれる
    float intensity = ring * (0.6f + 0.6f * lead) * gMeshParam.color.a;
    if (intensity < 0.004f) { discard; }

    float3 col = gMeshParam.color.rgb * intensity;
    output.color = float4(col, saturate(intensity)); // 加算プリマルチ
    return output;
}

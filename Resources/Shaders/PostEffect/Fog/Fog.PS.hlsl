#include "../FullScreen/FullScreen.hlsli"

// =====================================================================
// 大気フォグ
//   - distance fog (指数関数 + 開始距離)
//   - height fog (Y で密度が変化する低層霧)
//   - sun inscatter (光源方向を見ると霧色に太陽色が乗る)
// =====================================================================

struct FogParams
{
    float4x4 viewProjectionInverse; // depth → world 再構成用

    float3 cameraPos;
    float fogDensity; // 1/単位、distance fog の指数係数

    float3 fogColor;
    float fogStart; // 開始距離

    float3 sunDirection; // ライト方向（光が進む向き、normalized）
    float sunInscatterStrength; // 0で無効, 1で目立つ

    float3 sunColor;
    float skyFogClamp; // 空ピクセルのフォグ最大量 (0..1)

    float heightFogTop; // この Y より上は heightFog 寄与なし
    float heightFogBottom; // この Y より下で最大
    float heightFogDensity;
    float heightFogDistanceScale; // heightFog の距離依存スケール
};

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);

SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

ConstantBuffer<FogParams> gFog : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 sceneColor = gTexture.Sample(gSampler, input.texCoord);
    float ndcDepth = gDepthTexture.Sample(gSamplerPoint, input.texCoord);

    // UV → NDC.xy
    float2 ndcXY = float2(input.texCoord.x * 2.0f - 1.0f,
                          1.0f - input.texCoord.y * 2.0f);

    // depth + UV からワールド座標を復元
    float4 clipPos = float4(ndcXY, ndcDepth, 1.0f);
    float4 worldPos4 = mul(clipPos, gFog.viewProjectionInverse);
    float3 worldPos = worldPos4.xyz / worldPos4.w;

    // カメラからの方向・距離
    float3 toPixel = worldPos - gFog.cameraPos;
    float distance = length(toPixel);
    float3 viewDir = toPixel / max(distance, 1e-4f);

    // 空ピクセル判定（far plane に張り付いている）
    bool isSky = ndcDepth >= 0.9999f;

    // ====== distance fog (exponential) ======
    float distFromStart = max(0.0f, distance - gFog.fogStart);
    float distFog = 1.0f - exp(-gFog.fogDensity * distFromStart);

    // ====== height fog (低層の霧) ======
    // worldPos.y が heightFogTop に近いほど薄く、bottom に近いほど濃い
    float h = saturate((gFog.heightFogTop - worldPos.y)
                       / max(gFog.heightFogTop - gFog.heightFogBottom, 0.001f));
    float heightDensity = h * h * gFog.heightFogDensity;
    // 距離が長いほど積算されるイメージ
    float heightFog = 1.0f - exp(-heightDensity * distance * gFog.heightFogDistanceScale);

    // 合成（独立確率の論理和: a + b - a*b）
    float fogAmount = saturate(distFog + heightFog - distFog * heightFog);

    // 空はホワイトアウトしすぎないようにクランプ
    if (isSky)
    {
        fogAmount = min(fogAmount, gFog.skyFogClamp);
    }

    // ====== sun inscatter ======
    // 光源方向（-sunDirection が光源の出発点側）に視線が向くほど太陽色を載せる
    float cosSun = saturate(dot(viewDir, -gFog.sunDirection));
    float sunPower = pow(cosSun, 8.0f);
    float3 inscatter = gFog.sunColor * (sunPower * gFog.sunInscatterStrength);
    float3 fogColorFinal = gFog.fogColor + inscatter;

    // ====== 最終合成 ======
    output.color.rgb = lerp(sceneColor.rgb, fogColorFinal, fogAmount);
    output.color.a = sceneColor.a;
    return output;
}

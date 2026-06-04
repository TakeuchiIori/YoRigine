#include "../FullScreen/FullScreen.hlsli"

// =====================================================================
// God Rays (Crepuscular Rays / Light Shafts)
//   スクリーンスペースで sunUV から放射状に深度を march。
//   各サンプルが「空 (=far plane)」なら光を蓄積し、占有されていれば 0。
//   太陽方向に向かって光の筋が伸びる古典的なポストエフェクト。
//
//   sunUV / sunVisibility は CPU 側で計算して渡す。
//   - sunUV: 太陽が映る画面 UV (0..1)。画面外の場合もそれっぽい値が入る
//   - sunVisibility: 0=カメラの後ろ・完全に見えない / 1=正面
// =====================================================================

struct GodRaysParams
{
    float2 sunUV; // CPU 計算済みの太陽スクリーン UV
    float sunVisibility; // 0..1, カメラの後ろなら 0
    float density; // 各サンプル間隔のスケール (1.0 = 標準)

    float3 sunColor; // 光の色
    float weight; // 1サンプルあたりの寄与

    float decay; // 1サンプルあたり倍率 (0..1, 0.96 程度)
    float exposure; // 最終強度倍率
    int numSamples; // サンプル数 (32〜128)
    float skyThreshold; // 深度がこれ以上 = 空 (0.9999 程度)
};

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);

SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

ConstantBuffer<GodRaysParams> gGodRays : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 sceneColor = gTexture.Sample(gSampler, input.texCoord);

    // 太陽がカメラの後ろなら効果なし
    if (gGodRays.sunVisibility <= 0.001f)
    {
        output.color = sceneColor;
        return output;
    }

    // 現ピクセルから sunUV への march
    int numSamples = max(gGodRays.numSamples, 1);
    float2 deltaTexCoord = (input.texCoord - gGodRays.sunUV)
                          * (gGodRays.density / float(numSamples));

    float2 texCoord = input.texCoord;
    float illumination = 0.0f;
    float currentWeight = gGodRays.weight;

    // ループは [unroll(64)] で固定上限。numSamples で動的にカット
    [unroll(64)]
    for (int i = 0; i < 64; ++i)
    {
        if (i >= numSamples)
            break;

        texCoord -= deltaTexCoord;

        // 画面外のサンプルはクランプ (CLAMP サンプラなので端の深度になる)
        float2 sampleUV = saturate(texCoord);

        float depth = gDepthTexture.Sample(gSamplerPoint, sampleUV);
        // 「空」かどうか (深度が far に張り付いている)
        float skyMask = (depth >= gGodRays.skyThreshold) ? 1.0f : 0.0f;

        illumination += skyMask * currentWeight;
        currentWeight *= gGodRays.decay;
    }

    illumination *= gGodRays.exposure * gGodRays.sunVisibility;

    // 加算合成
    float3 godRays = illumination * gGodRays.sunColor;
    output.color.rgb = sceneColor.rgb + godRays;
    output.color.a = sceneColor.a;
    return output;
}

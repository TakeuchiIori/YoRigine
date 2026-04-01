
#include "ProceduralMesh.hlsli"

ConstantBuffer<MeshTrailParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 texcoord = input.texcoord;
    float  age      = saturate(input.age); // 0=新しい, 1=古い(消えかけ)

    //--------------------------------------------------
    // 幅方向エッジフェード
    // 刃先(texcoord.x=0)と根本(texcoord.x=1)を滑らかに消す
    //--------------------------------------------------
    float widthFade = EdgeFade(texcoord.x, gMeshParam.softness * 0.5f + 0.05f);

    //--------------------------------------------------
    // 時間方向フェード
    // 古い頂点(age=1)ほど透明になる
    //--------------------------------------------------
    float ageFade = pow(1.0f - age, 1.5f); // ゆっくり消える感じに

    //--------------------------------------------------
    // UV 歪み (trailDistortion > 0 のときのみ)
    //--------------------------------------------------
    float2 distortedUV = texcoord;
    if (gMeshParam.distortion > 0.001f)
    {
        float2 noiseUV = texcoord * 2.0f + float2(gMeshParam.time * 0.3f, gMeshParam.time * 0.15f);
        float2 noiseVal = gTexNoise.Sample(gSampler, noiseUV).rg * 2.0f - 1.0f;
        distortedUV += noiseVal * gMeshParam.distortion * (1.0f - texcoord.y); // 先端ほど強く
    }

    //--------------------------------------------------
    // グラデーションカラー
    // 刃先(x=0)→ trailColorInner, 根本(x=1)→ trailColorOuter
    // gTexRamp があればそちらを優先
    //--------------------------------------------------
    float4 rampSample = gTexRamp.Sample(gSamplerClamp, float2(distortedUV.x, 0.5f));
    float  rampMask   = saturate(dot(rampSample.rgb, float3(0.333f, 0.333f, 0.333f)));
    float4 lerpColor  = lerp(gMeshParam.colorInner, gMeshParam.colorOuter, distortedUV.x);
    float4 baseColor  = lerp(lerpColor, rampSample * lerpColor, rampMask < 0.99f ? 1.0f : 0.0f);

    // 頂点カラーとの乗算
    baseColor *= input.color;

    //--------------------------------------------------
    // 中央グロー
    //--------------------------------------------------
    float centerHighlight = 1.0f - abs(texcoord.x * 2.0f - 1.0f);
    centerHighlight = pow(centerHighlight, 2.0f);
    float3 glow = baseColor.rgb * centerHighlight * gMeshParam.glowPower;

    //--------------------------------------------------
    //  最終合成
    //  加算ブレンドなので alpha を rgb に乗じて出力
    //--------------------------------------------------
    float  alpha = baseColor.a * widthFade * ageFade;
    float3 color = baseColor.rgb + glow;

    output.color = float4(color * alpha, alpha);
    return output;
}

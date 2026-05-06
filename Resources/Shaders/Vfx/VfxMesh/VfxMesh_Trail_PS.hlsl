#include "VfxMesh_Common.hlsli"

ConstantBuffer<MeshTrailParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv  = input.texcoord;
    float  age = saturate(input.age); // 0=新しい, 1=古い(消えかけ)

    //==================================================
    // 1. ノイズ UV 歪み (マルチオクターブ FBM)
    //    UE の Noise ノード相当
    //==================================================
    float2 distortedUV = uv;
    float  noiseSample = 0.5f;
    if (gMeshParam.distortion > 0.001f)
    {
        int oct = clamp((int)gMeshParam.noiseOctaves, 1, 4);
        float2 noiseUV = uv * float2(2.0f, 4.0f)
                       + float2(gMeshParam.time * 0.25f, gMeshParam.time * 0.1f);

        // テクスチャが白(デフォルト)の場合は FBM にフォールバック
        float4 noiseTex = gTexNoise.Sample(gSampler, noiseUV);
        float  texLum   = dot(noiseTex.rgb, float3(0.299f, 0.587f, 0.114f));
        // テクスチャが有効(白一色でない)かを判定
        bool   hasTex   = (texLum < 0.99f);

        float2 noiseVec;
        if (hasTex)
        {
            noiseVec = noiseTex.rg * 2.0f - 1.0f;
        }
        else
        {
            // FBM で代替 (テクスチャ不要でもそれなりに動く)
            noiseVec.x = FBM(noiseUV,              oct) * 2.0f - 1.0f;
            noiseVec.y = FBM(noiseUV + 5.2f, oct) * 2.0f - 1.0f;
        }
        noiseSample = texLum;

        // 先端(x=0)ほど歪みを強く → 剣閃の炎っぽさ
        float distortStrength = gMeshParam.distortion * (1.0f - uv.x * 0.5f);
        distortedUV += noiseVec * distortStrength;
    }
    distortedUV = saturate(distortedUV);

    //==================================================
    // 2. グラデーションカラー (Ramp LUT)
    //    UE の Color over Life / Gradient ノード相当
    //==================================================
    float4 rampSample  = gTexRamp.Sample(gSamplerClamp, float2(distortedUV.x, 0.5f));
    float  rampLum     = dot(rampSample.rgb, float3(0.333f, 0.333f, 0.333f));
    bool   hasRamp     = (rampLum < 0.99f);

    float4 lerpColor = lerp(gMeshParam.colorInner, gMeshParam.colorOuter, distortedUV.x);
    float4 baseColor;
    if (hasRamp)
    {
        // ランプテクスチャをカラーに乗算 (UE の Multiply ノード)
        baseColor = rampSample * lerpColor;
    }
    else
    {
        baseColor = lerpColor;
    }

    //==================================================
    // 3. カラーウェーブ (UE の Sine/Time ノード相当)
    //    生きているような脈動を追加
    //==================================================
    if (gMeshParam.colorWaveAmp > 0.001f)
    {
        float wave = sin(uv.y * gMeshParam.colorWaveFreq + gMeshParam.time * 5.0f) * 0.5f + 0.5f;
        // colorInner と colorOuter の間でパルス
        float4 pulseColor = lerp(gMeshParam.colorInner, gMeshParam.colorOuter, wave);
        baseColor = lerp(baseColor, pulseColor, gMeshParam.colorWaveAmp);
    }

    // 頂点カラー (ageフェード) との乗算
    baseColor *= input.color;

    //==================================================
    // 4. エッジフェード & シャープネス
    //    UE の EdgeFade マスク相当
    //    trailSharpness: 高いほどエッジがくっきり
    //==================================================
    float softEdge  = gMeshParam.softness * 0.5f + 0.05f;
    float widthFade = EdgeFade(distortedUV.x, softEdge);
    // シャープネスで冪乗
    widthFade = pow(widthFade, max(0.1f, gMeshParam.trailSharpness * 0.5f));

    //==================================================
    // 5. 時間フェード (age → 透明化)
    //    UE の Color over Life Alpha 相当
    //==================================================
    // 1.5乗でゆっくり消える → 根本に密度感
    float ageFade = pow(1.0f - age, 1.5f);

    //==================================================
    // 6. 中心コアグロー (UE の RadialGradient + Multiply)
    //    トレイル中心に輝く白/明るい芯を追加
    //==================================================
    float center   = 1.0f - abs(uv.x * 2.0f - 1.0f);
    float coreGlow = pow(center, 3.0f) * gMeshParam.glowPower;
    // 芯は白く発光 (加算ブレンドで HDR bloom 誘発)
    float3 coreColor = baseColor.rgb * coreGlow + coreGlow * 0.3f;

    //==================================================
    // 7. ★ フレネルエッジグロー (UE の Fresnel ノード相当)
    //    エッジをリムカラーで縁取り
    //==================================================
    float edgeT       = 1.0f - abs(uv.x * 2.0f - 1.0f); // 0=エッジ, 1=中心
    float fresnel     = FresnelEdge(1.0f - edgeT, max(1.0f, gMeshParam.fresnelStrength * 2.0f));
    float3 rimGlow    = gMeshParam.rimColor.rgb * fresnel
                      * gMeshParam.fresnelStrength
                      * ageFade;

    //==================================================
    // 8. ★ エネルギーライン (UE Niagara ライトニングリボン)
    //    トレイル長方向に走る明るい細線
    //==================================================
    float3 energyColor = float3(0.0f, 0.0f, 0.0f);
    if (gMeshParam.energyIntensity > 0.001f)
    {
        // エッジ付近にのみ現れる (center が小さい部分 = エッジ)
        float energyMask  = pow(1.0f - center, 1.5f) * widthFade * ageFade;

        float energyVal   = EnergyLines(
            float2(uv.x, uv.y + gMeshParam.time * 0.3f),
            gMeshParam.time,
            6.0f,                    // ライン本数
            gMeshParam.energySpeed
        );

        // エネルギーラインの色: rimColor を基調にしつつ白に近い
        energyColor = lerp(gMeshParam.rimColor.rgb, float3(1, 1, 1), 0.5f)
                    * energyVal
                    * gMeshParam.energyIntensity
                    * energyMask;
    }

    //==================================================
    // 9. ★ スパークル (UE の TemporalNoise / Sparkle)
    //    ランダムにきらめく光点
    //==================================================
    float3 sparkleColor = float3(0.0f, 0.0f, 0.0f);
    if (gMeshParam.sparkleAmount > 0.001f)
    {
        float2 sparkleUV = uv + float2(0.0f, gMeshParam.time * gMeshParam.uvScrollSpeed);
        float  sp        = Sparkle(sparkleUV, gMeshParam.time, gMeshParam.sparkleSpeed, 8.0f);

        // スパークルは白 + rimColor のブレンド
        sparkleColor = lerp(baseColor.rgb, float3(1.5f, 1.5f, 1.5f), 0.8f)
                     * sp
                     * gMeshParam.sparkleAmount
                     * widthFade
                     * ageFade;
    }

    //==================================================
    // 10. 最終合成
    //     加算ブレンドなので alpha を rgb に乗じて出力
    //     UE の EmissiveColor 出力と同等
    //==================================================
    float  alpha  = baseColor.a * widthFade * ageFade;

    float3 finalRGB =
          baseColor.rgb     // ベースカラー
        + coreColor          // 中心グロー
        + rimGlow            // フレネルリム
        + energyColor        // エネルギーライン
        + sparkleColor;      // スパークル

    // 加算ブレンド用: pre-multiplied alpha で出力
    output.color = float4(finalRGB * alpha, alpha);
    return output;
}

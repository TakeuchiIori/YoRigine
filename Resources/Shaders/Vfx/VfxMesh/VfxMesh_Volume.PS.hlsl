#include "VfxMesh_Common.hlsli"

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<LightVolumeParams> gMeshParam : register(b1);


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 texcoord = input.texcoord;
    float  age      = saturate(input.age); // 0=中心, 1=Z端

    //--------------------------------------------------
    // OBB 各軸方向のエッジフェード
    //--------------------------------------------------
    float fadeX = EdgeFade(texcoord.x, gMeshParam.edgeFade);
    float fadeY = EdgeFade(texcoord.y, gMeshParam.edgeFade);
    float fadeZ = 1.0f - smoothstep(1.0f - gMeshParam.edgeFade, 1.0f, age);

    float shapeFade = fadeX * fadeY * fadeZ;

    //--------------------------------------------------
    // カメラ近接フェード
    // Object3d_PS の toEye 計算と同じ gCamera.worldPosition を使用
    //--------------------------------------------------
    float camDist   = length(input.worldPosition - gCamera.worldPosition);
    float depthFade = saturate(camDist / max(gMeshParam.depthFade, 0.001f));

    //--------------------------------------------------
    // ノイズによる内部揺らぎ
    //--------------------------------------------------
    float noiseMask = 1.0f;
    if (gMeshParam.noiseStrength > 0.001f)
    {
        float2 noiseUV  = texcoord * gMeshParam.noiseTiling + float2(gMeshParam.time * 0.05f, gMeshParam.time * 0.03f);
        float  noiseVal = gTexNoise.Sample(gSampler, noiseUV).r;
        float  edgeness = 1.0f - min(fadeX, min(fadeY, fadeZ));
        noiseMask = lerp(1.0f, noiseVal, gMeshParam.noiseStrength * edgeness);
    }

    //--------------------------------------------------
    // 中心グロー
    //--------------------------------------------------
    float centerX   = 1.0f - abs(texcoord.x * 2.0f - 1.0f);
    float centerY   = 1.0f - abs(texcoord.y * 2.0f - 1.0f);
    float centerZ   = 1.0f - age;
    float centerGlow = pow(centerX * centerY * centerZ, 1.5f);

    //--------------------------------------------------
    // ビーム: 中心の芯を強く、外側を柔らかく落とす
    //--------------------------------------------------
    float2 beamUV = texcoord * 2.0f - 1.0f;
    float  beamR  = length(beamUV);
    float  beamRadius = max(gMeshParam.beamRadius, 0.001f);
    float  beamCore = pow(saturate(1.0f - beamR / beamRadius), max(gMeshParam.beamPower, 0.001f));
    float  beamMask = lerp(1.0f, beamCore, saturate(gMeshParam.beamStrength));

    //--------------------------------------------------
    // 最終カラー合成
    // 加算ブレンドなので alpha を rgb に乗じて出力
    //--------------------------------------------------
    float4 baseColor = gMeshParam.color * input.color;
    float  intensity = baseColor.a * gMeshParam.color.a;

    float3 color = baseColor.rgb * (1.0f + centerGlow + beamCore * gMeshParam.beamGlow * gMeshParam.beamStrength);
    float  alpha = intensity * shapeFade * depthFade * noiseMask * beamMask;

    output.color = float4(color * alpha, alpha);
    return output;
}

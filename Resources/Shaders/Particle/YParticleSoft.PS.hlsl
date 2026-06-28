#include "YParticle.hlsli"
#include "../Light/Light.hlsli"

// ソフトパーティクル専用 PS。
// 通常の YParticle.PS にシーン深度との比較（手動遮蔽＋接地フェード）を加えたもの。
// このシェーダは softParticle を有効にしたシステムのバッチでのみ使われる。
// 深度バッファは「深度なしRT」状態で SRV として渡される（DirectXCommon::BeginParticleSoftDepth）。

// MaterialLighting
struct MaterialLight
{
    uint enableLighting;
    uint enableSpecular;
    uint enableEnvironment;
    uint isHalfVector;
    float shininess;
    float environmentCoefficient;
    float padding[2];
};

// カメラ情報
struct CameraForGPU
{
    float3 worldPosition;
    float4x4 viewProjection;
};

// ソフトパーティクル用パラメータ
struct SoftParticleParams
{
    float nearZ;        // カメラ近クリップ
    float farZ;         // カメラ遠クリップ
    float fadeDistance; // 接地面で減衰しきる距離（ワールド単位）
    float padding;
};

ConstantBuffer<MaterialLight> gMaterialLight : register(b1);
ConstantBuffer<DirectionalLightData> gDirectionalLight : register(b2);
ConstantBuffer<CameraForGPU> gCamera : register(b3);
ConstantBuffer<SoftParticleParams> gSoftParticle : register(b4);

Texture2D<float4> gTexture : register(t1);
Texture2D<float> gSceneDepth : register(t2); // シーン深度（MainDepth の SRV）
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 非線形深度[0,1] → ビュー空間の線形深度（標準的な透視射影 0..1 を想定）
float LinearizeDepth(float z, float n, float f)
{
    return (n * f) / (f - z * (f - n));
}

float3 CalculateDirectionalLight(float3 worldPos, float3 normal, float3 toEye)
{
    float3 lightDir = normalize(-gDirectionalLight.direction);
    float NdotL = saturate(dot(normal, lightDir));
    float3 diffuseColor = gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;

    float3 specularColor = float3(0, 0, 0);
    if (gMaterialLight.enableSpecular != 0)
    {
        float3 reflectDir;
        if (gMaterialLight.isHalfVector != 0)
        {
            reflectDir = normalize(lightDir + toEye);
        }
        else
        {
            reflectDir = reflect(-lightDir, normal);
        }
        float specularPow = saturate(dot(reflectDir, normal));
        specularPow = pow(specularPow, gMaterialLight.shininess);
        specularColor = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity;
    }
    return diffuseColor + specularColor;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 baseColor = textureColor * input.color;

    if (baseColor.a < 0.01f)
    {
        discard;
    }

    //=================================================================
    // ソフトパーティクル：シーン深度と比較
    //  - 粒がシーンより奥なら破棄（手動遮蔽。深度テスト無しで描くため）
    //  - 接地面に近いほど α を減衰（硬い切り口を消す）
    //=================================================================
    {
        float sceneZ = gSceneDepth.Load(int3(input.position.xy, 0)).r;
        float n = gSoftParticle.nearZ;
        float f = gSoftParticle.farZ;

        float sceneLin = LinearizeDepth(sceneZ, n, f);
        float selfLin = LinearizeDepth(input.position.z, n, f);

        float diff = sceneLin - selfLin;
        if (diff <= 0.0f)
        {
            discard; // シーンより奥 = 遮蔽
        }

        float fade = saturate(diff / max(gSoftParticle.fadeDistance, 0.0001f));
        // rgb にも fade を掛ける（premultiplied）。
        // 加算合成(SrcBlend=ONE)は alpha を見ないため、rgb を減衰させないとフェードが効かない。
        // 通常(アルファ)ブレンドでも rgb*alpha が減って正しく薄くなる。
        baseColor.rgb *= fade;
        baseColor.a *= fade;
        if (fade < 0.001f)
        {
            discard;
        }
    }

    // ライティング無効ならそのまま出力
    if (gMaterialLight.enableLighting == 0)
    {
        output.color = baseColor;
        return output;
    }

    float3 normal = normalize(input.normal);
    float3 worldPos = input.worldPosition;
    float3 toEye = normalize(gCamera.worldPosition - worldPos);

    float3 ambient = float3(0.1f, 0.1f, 0.1f);
    float3 lighting = ambient;
    lighting += CalculateDirectionalLight(worldPos, normal, toEye);

    if (gMaterialLight.enableEnvironment != 0)
    {
        lighting *= gMaterialLight.environmentCoefficient;
    }

    output.color = float4(baseColor.rgb * lighting, baseColor.a);
    return output;
}

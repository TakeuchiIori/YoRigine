#include "Effect.hlsli"
#include "../Shadow/Cascade.hlsli"

// ==========================================
//  構造体定義 (Object3d由来 + Effect拡張)
// ==========================================

struct MaterialUV
{
    float4x4 uvTransform;
};

struct MaterialColor
{
    float4 color : SV_TARGET0;
};

struct MaterialLight
{
    int enableLighting;
    int enableSpecular;
    int enableEnvironment;
    int isHalfVector;
    float shininess;
    float environmentCoeffcient;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    int isEnableDirectionalLighting;
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    int isEnablePointLight;
    float radius;
    float decay;
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    int isEnableSpotLight;
};


// カメラ座標
struct Camera
{
    float3 worldPosition;
    float4x4 viewProjection;
};

// ==========================================
//  定数バッファ & テクスチャ
// ==========================================

ConstantBuffer<CascadeShadow> gCascadeShadow : register(b0);
ConstantBuffer<MaterialUV> gMaterialUV : register(b1);
ConstantBuffer<MaterialColor> gMaterialColor : register(b2);
ConstantBuffer<MaterialLight> gMaterialLight : register(b3);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b4);
ConstantBuffer<PointLight> gPointLight : register(b5);
ConstantBuffer<SpotLight> gSpotLight : register(b6);
ConstantBuffer<Camera> gCamera : register(b7);


Texture2D<float4> gTexture : register(t1);
TextureCube<float4> gEnvironmentTexture : register(t2);
Texture2DArray<float> gShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ==========================================
//  メイン関数
// ==========================================

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV座標変換とテクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialUV.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float4 baseColor = input.color * textureColor;

    // 透明なピクセルはここで描画を打ち切る (影計算の無駄を省く)
    if (baseColor.a <= 0.0)
        discard;

    float3 finalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 finalSpecular = float3(0.0f, 0.0f, 0.0f);

    if (gMaterialLight.enableLighting)
    {
        //========================================== シャドウマップ（カスケード） ==========================================//
        const float shadowMapSize = 2048.0f;
        float shadow = SampleCascadeShadow(gShadowMap, gShadowSampler, gCascadeShadow,
                                           input.worldPosition, shadowMapSize);
        float shadowFactor = max(shadow, 0.3f);

        // カメラ視線ベクトル
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        //========================================== ディレクショナルライト ==========================================//
        if (gDirectionalLight.isEnableDirectionalLighting)
        {
            float NdotL = max(dot(normalize(input.normal), -gDirectionalLight.direction), 0.0f);
            float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
            float3 diffuseDirectional = gMaterialColor.color.rgb * baseColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
            
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = max(dot(normalize(input.normal), halfVector), 0.0f);
            float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(saturate(NdotH), gMaterialLight.shininess);

            if (gMaterialLight.enableSpecular != 0)
                finalSpecular += specularDirectional * shadowFactor;
            finalDiffuse += diffuseDirectional * shadowFactor;
        }

        //========================================== ポイントライト ==========================================//
        if (gPointLight.isEnablePointLight)
        {
            float3 pointLightDirection = normalize(gPointLight.position - input.worldPosition);
            float distance = length(gPointLight.position - input.worldPosition);
            float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);
            
            float NdotLPoint = max(dot(normalize(input.normal), pointLightDirection), 0.0f);
            float3 diffusePoint = gMaterialColor.color.rgb * baseColor.rgb * gPointLight.color.rgb * NdotLPoint * gPointLight.intensity * factor;
            
            float3 halfVectorPoint = normalize(pointLightDirection + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * pow(saturate(NdotHPoint), gMaterialLight.shininess) * factor;

            if (gMaterialLight.enableSpecular != 0)
                finalSpecular += specularPoint * shadowFactor;
            finalDiffuse += diffusePoint * shadowFactor;
        }

        //========================================== スポットライト ==========================================//
        if (gSpotLight.isEnableSpotLight)
        {
            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
            float distanceToSurface = length(gSpotLight.position - input.worldPosition);
            float distanceDecay = pow(saturate(-distanceToSurface / gSpotLight.distance + 1.0f), gSpotLight.decay);
            float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
            float angleFalloff = saturate((cosAngle - gSpotLight.cosFalloffStart) / (gSpotLight.cosAngle - gSpotLight.cosFalloffStart));
            float falloffFactor = angleFalloff * distanceDecay;

            float NdotLPoint = max(dot(normalize(input.normal), -spotLightDirectionOnSurface), 0.0f);
            float3 diffusePoint = gMaterialColor.color.rgb * baseColor.rgb * gSpotLight.color.rgb * NdotLPoint * gSpotLight.intensity * falloffFactor;

            float3 halfVectorPoint = normalize(-spotLightDirectionOnSurface + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = gSpotLight.color.rgb * gSpotLight.intensity * pow(saturate(NdotHPoint), gMaterialLight.shininess) * falloffFactor;

            if (gMaterialLight.enableSpecular != 0)
                finalSpecular += specularPoint * shadowFactor;
            finalDiffuse += diffusePoint * shadowFactor;
        }

        //========================================== 環境マップ ==========================================//
        float3 environmentColor = float3(0.0f, 0.0f, 0.0f);
        if (gMaterialLight.enableEnvironment)
        {
            float3 incident = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectionVector = reflect(incident, normalize(input.normal));
            environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionVector).rgb * gMaterialLight.environmentCoeffcient;
        }

        // ライティング結果の合成
        output.color.rgb = finalDiffuse + finalSpecular + environmentColor;
    }
    else
    {
        //========================================== ライティングなし ==========================================//
        // エフェクトの発光分もここで加算
        output.color.rgb = gMaterialColor.color.rgb * baseColor.rgb;
    }

    // 最終アルファ
    output.color.a = gMaterialColor.color.a * baseColor.a;

    return output;
}
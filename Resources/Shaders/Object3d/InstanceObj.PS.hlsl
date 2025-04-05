#include "Object3d.hlsli"
struct Material
{
    float4 color : SV_TARGET0;
    int enableLighting;
    float4x4 uvTransform;
    float shininess;
    int enableSpecular;
    int isHalfVector;
};

struct MaterialInstances
{
    float4 color : SV_TARGET0;
    int enableLighting;
    float4x4 uvTransform;
    float shininess;
    int enableSpecular;
    int isHalfVector;
};


struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き
    float intensity; // 輝度
    int isEnableDirectionalLighting;
};
struct Camera
{
    float3 worldPosition;
};
struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    int isEnablePointLight;
    float radius; // ライトの届く最大距離
    float decay; // 減衰率
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
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
   
};

//ConstantBuffer<Material> gMaterialInstances[instanceID] : register(b0);
StructuredBuffer <MaterialInstances> gMaterialInstances : register(t2);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
PixelShaderOutput main(VertexShaderOutput input,uint instanceID : SV_InstanceID)
{
    PixelShaderOutput output;

    // UV座標変換とテクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialInstances[instanceID].uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 初期化
    float3 finalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 finalSpecular = float3(0.0f, 0.0f, 0.0f);

    if (gMaterialInstances[instanceID].enableLighting)
    {
        // カメラ視線ベクトル
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        //===========================================================//
        //                   ディレクショナルライトの計算               
        //===========================================================//
        
        if (gDirectionalLight.isEnableDirectionalLighting)
        {
            // 拡散反射
            float NdotL = max(dot(normalize(input.normal), -gDirectionalLight.direction), 0.0f);
            float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
            float3 diffuseDirectional = gMaterialInstances[instanceID].color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
            // 鏡面反射 (Blinn-Phong)
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = max(dot(normalize(input.normal), halfVector), 0.0f);
            float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(saturate(NdotH), gMaterialInstances[instanceID].shininess);

            // フラグで有効化
            if (gMaterialInstances[instanceID].enableSpecular != 0)
            {
                finalSpecular += specularDirectional;
            }
            finalDiffuse += diffuseDirectional;
        }

        //===========================================================//
        //                      ポイントライトの計算               
        //===========================================================//
        
        if (gPointLight.isEnablePointLight)
        {
            // ライト方向ベクトル
            float3 pointLightDirection = normalize(gPointLight.position - input.worldPosition);
            
            float distance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
            float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay); //指数によるコントロール
            
            // 拡散反射
            float NdotLPoint = max(dot(normalize(input.normal), pointLightDirection), 0.0f);
            float3 diffusePoint = gMaterialInstances[instanceID].color.rgb * textureColor.rgb * gPointLight.color.rgb * NdotLPoint * gPointLight.intensity * factor;
            // 鏡面反射 (Blinn-Phong)
            float3 halfVectorPoint = normalize(pointLightDirection + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * pow(saturate(NdotHPoint), gMaterialInstances[instanceID].shininess) * factor;
        
            // フラグで有効化
            if (gMaterialInstances[instanceID].enableSpecular != 0)
            {
                finalSpecular += specularPoint;
            }
            finalDiffuse += diffusePoint;
        }
        
        //===========================================================//
        //                      スポットライトの計算               
        //===========================================================//
        
        if (gSpotLight.isEnableSpotLight)
        {
            // スポットライトの方向ベクトル
            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);

            // ライトとの距離
            float distanceToSurface = length(gSpotLight.position - input.worldPosition);

            // 距離による減衰率 (逆距離の減衰計算)
            float distanceDecay = pow(saturate(-distanceToSurface / gSpotLight.distance + 1.0f), gSpotLight.decay);

            // スポットライトの角度による減衰 (Falloff開始角度を考慮)
            float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
            float angleFalloff = saturate((cosAngle - gSpotLight.cosFalloffStart) / (gSpotLight.cosAngle - gSpotLight.cosFalloffStart));

            // 総減衰係数
            float falloffFactor = angleFalloff * distanceDecay;

            // 拡散反射 (NdotL)
            float NdotLPoint = max(dot(normalize(input.normal), -spotLightDirectionOnSurface), 0.0f);
            float3 diffusePoint = gMaterialInstances[instanceID].color.rgb * textureColor.rgb * gSpotLight.color.rgb * NdotLPoint * gSpotLight.intensity * falloffFactor;

            // 鏡面反射 (Blinn-Phong)
            float3 halfVectorPoint = normalize(-spotLightDirectionOnSurface + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = gSpotLight.color.rgb * gSpotLight.intensity * pow(saturate(NdotHPoint), gMaterialInstances[instanceID].shininess) * falloffFactor;

            // スペキュラー反射の有効化
            if (gMaterialInstances[instanceID].enableSpecular != 0)
            {
                finalSpecular += specularPoint;
            }
            finalDiffuse += diffusePoint;
        }

        // 最終的な色を合成
        output.color.rgb = finalDiffuse + finalSpecular;
    }
    else
    {
        // ライティングなしの場合
        output.color.rgb = gMaterialInstances[instanceID].color.rgb * textureColor.rgb;
    }

    // アルファ値の設定
    output.color.a = gMaterialInstances[instanceID].color.a * textureColor.a;

    return output;
}

#include "YParticle.hlsli"

//=================================================================
// マテリアル構造体（C++のMaterialColorクラスに対応）
//=================================================================

struct MaterialColorData
{
    float4 color; // 16バイト
};

struct MaterialUV
{
    float4x4 uvTransform; // 64バイト
};
// MaterialLighting
struct MaterialLight
{
    uint enableLighting; // 4バイト
    uint enableSpecular; // 4バイト
    uint enableEnvironment; // 4バイト
    uint isHalfVector; // 4バイト
    float shininess; // 4バイト
    float environmentCoefficient; // 4バイト
    float padding[2]; // 8バイト（合計32バイト）
};

// 平行光源
struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き
    float intensity; // 輝度
};

// ポイントライト
struct PointLight
{
    float4 color; // 色
    float3 position; // 位置
    float intensity; // 輝度
    uint enablePointLight; // 有効フラグ
    float radius; // 最大距離
    float decay; // 減衰率
    float padding[2];
};

// スポットライト
struct SpotLight
{
    float4 color; // 色
    float3 position; // 位置
    float intensity; // 輝度
    float3 direction; // 方向
    float distance; // 最大距離
    float decay; // 減衰率
    float cosAngle; // 余弦
    float cosFalloffStart; // フォールオフ開始角度
    uint enableSpotLight; // 有効フラグ
    float padding[2];
};

// カメラ情報
struct CameraForGPU
{
    float3 worldPosition; // カメラのワールド座標
    float4x4 viewProjection; // ビュー行列
};

//=================================================================
// 定数バッファ
//=================================================================

// マテリアル関連
ConstantBuffer<MaterialColorData> gMaterialColor : register(b0);
ConstantBuffer<MaterialUV> gMaterialUV : register(b1);

// ライティング関連
ConstantBuffer<MaterialLight> gMaterialLight : register(b2);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
ConstantBuffer<CameraForGPU> gCamera : register(b6);
//=================================================================
// テクスチャとサンプラー
//=================================================================

Texture2D<float4> gTexture : register(t1);
SamplerState gSampler : register(s0);

//=================================================================
// ピクセルシェーダー出力
//=================================================================

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

//=================================================================
// ライティング計算関数
//=================================================================

/// <summary>
/// 平行光源の計算
/// </summary>
float3 CalculateDirectionalLight(float3 worldPos, float3 normal, float3 toEye)
{
    // ライトの方向（光源から物体への方向）
    float3 lightDir = normalize(-gDirectionalLight.direction);
    
    // 拡散反射の計算
    float NdotL = saturate(dot(normal, lightDir));
    
    // Diffuse成分
    float3 diffuseColor = gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
    
    // Specular成分（有効な場合のみ）
    float3 specularColor = float3(0, 0, 0);
    if (gMaterialLight.enableSpecular != 0)
    {
        float3 reflectDir;
        
        if (gMaterialLight.isHalfVector != 0)
        {
            // Half Vector方式（Blinn-Phong）
            float3 halfVector = normalize(lightDir + toEye);
            reflectDir = halfVector;
        }
        else
        {
            // Phong反射方式
            reflectDir = reflect(-lightDir, normal);
        }
        
        // スペキュラー強度の計算
        float specularPow = saturate(dot(reflectDir, normal));
        specularPow = pow(specularPow, gMaterialLight.shininess);
        specularColor = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity;
    }
    
    return diffuseColor + specularColor;
}

/// <summary>
/// ポイントライトの計算
/// </summary>
float3 CalculatePointLight(float3 worldPos, float3 normal, float3 toEye)
{
    // ライトが無効な場合は早期リターン
    if (gPointLight.enablePointLight == 0)
        return float3(0, 0, 0);
    
    // ライトへの方向と距離
    float3 lightDir = gPointLight.position - worldPos;
    float distance = length(lightDir);
    
    // 距離減衰チェック（範囲外なら影響なし）
    if (distance > gPointLight.radius)
        return float3(0, 0, 0);
    
    lightDir = normalize(lightDir);
    
    // 拡散反射
    float NdotL = saturate(dot(normal, lightDir));
    
    // 距離減衰の計算
    float attenuation = pow(saturate(1.0f - (distance / gPointLight.radius)), gPointLight.decay);
    
    // Diffuse成分
    float3 diffuseColor = gPointLight.color.rgb * NdotL * gPointLight.intensity * attenuation;
    
    // Specular成分（有効な場合のみ）
    float3 specularColor = float3(0, 0, 0);
    if (gMaterialLight.enableSpecular != 0)
    {
        float3 reflectDir;
        
        if (gMaterialLight.isHalfVector != 0)
        {
            // Half Vector方式
            float3 halfVector = normalize(lightDir + toEye);
            reflectDir = halfVector;
        }
        else
        {
            // Phong反射方式
            reflectDir = reflect(-lightDir, normal);
        }
        
        float specularPow = saturate(dot(reflectDir, normal));
        specularPow = pow(specularPow, gMaterialLight.shininess);
        specularColor = gPointLight.color.rgb * specularPow * gPointLight.intensity * attenuation;
    }
    
    return diffuseColor + specularColor;
}

/// <summary>
/// スポットライトの計算
/// </summary>
float3 CalculateSpotLight(float3 worldPos, float3 normal, float3 toEye)
{
    // ライトが無効な場合は早期リターン
    if (gSpotLight.enableSpotLight == 0)
        return float3(0, 0, 0);
    
    // ライトへの方向と距離
    float3 lightDir = gSpotLight.position - worldPos;
    float distance = length(lightDir);
    
    // 距離チェック（範囲外なら影響なし）
    if (distance > gSpotLight.distance)
        return float3(0, 0, 0);
    
    lightDir = normalize(lightDir);
    
    // スポットライトの角度減衰計算
    float cosAngle = dot(lightDir, normalize(-gSpotLight.direction));
    float falloff = saturate((cosAngle - gSpotLight.cosAngle) /
                             (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
    
    // 範囲外なら影響なし
    if (falloff <= 0.0f)
        return float3(0, 0, 0);
    
    // 拡散反射
    float NdotL = saturate(dot(normal, lightDir));
    
    // 距離減衰
    float attenuation = pow(saturate(1.0f - (distance / gSpotLight.distance)), gSpotLight.decay);
    
    // Diffuse成分
    float3 diffuseColor = gSpotLight.color.rgb * NdotL * gSpotLight.intensity *
                          attenuation * falloff;
    
    // Specular成分（有効な場合のみ）
    float3 specularColor = float3(0, 0, 0);
    if (gMaterialLight.enableSpecular != 0)
    {
        float3 reflectDir;
        
        if (gMaterialLight.isHalfVector != 0)
        {
            // Half Vector方式
            float3 halfVector = normalize(lightDir + toEye);
            reflectDir = halfVector;
        }
        else
        {
            // Phong反射方式
            reflectDir = reflect(-lightDir, normal);
        }
        
        float specularPow = saturate(dot(reflectDir, normal));
        specularPow = pow(specularPow, gMaterialLight.shininess);
        specularColor = gSpotLight.color.rgb * specularPow * gSpotLight.intensity *
                        attenuation * falloff;
    }
    
    return diffuseColor + specularColor;
}

//=================================================================
// メインピクセルシェーダー
//=================================================================

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換を適用してテクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialUV.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 基本色の計算
    // 頂点カラー × テクスチャカラー × マテリアルカラー
    float4 baseColor = gMaterialColor.color * textureColor * input.color;
    
    // アルファテスト（完全に透明なピクセルは破棄）
    if (baseColor.a < 0.01f)
    {
        discard;
    }
    
    // ライティングが無効な場合はそのまま出力
    if (gMaterialLight.enableLighting == 0)
    {
        output.color = baseColor;
        return output;
    }
    
    //=================================================================
    // ライティング計算
    //=================================================================
    
    // 法線を正規化
    float3 normal = normalize(input.normal);
    
    // ワールド座標
    float3 worldPos = input.worldPosition;
    
    // カメラへの方向ベクトル
    float3 toEye = normalize(gCamera.worldPosition - worldPos);
    
    // 環境光（アンビエント）
    float3 ambient = float3(0.1f, 0.1f, 0.1f);
    
    // 各ライトの影響を計算して合成
    float3 lighting = ambient;
    lighting += CalculateDirectionalLight(worldPos, normal, toEye);
    lighting += CalculatePointLight(worldPos, normal, toEye);
    lighting += CalculateSpotLight(worldPos, normal, toEye);
    
    // 環境マッピング（有効な場合）
    if (gMaterialLight.enableEnvironment != 0)
    {
        // 反射ベクトルを計算
        float3 reflectDir = reflect(-toEye, normal);
        
        // 環境マッピングは別途実装が必要
        // ここでは簡易的に環境係数のみ適用
        lighting *= gMaterialLight.environmentCoefficient;
    }
    
    // 最終色の計算（ライティングを適用）
    output.color = float4(baseColor.rgb * lighting, baseColor.a);
    
    return output;
}

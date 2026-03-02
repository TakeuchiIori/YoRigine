#include "Particle.hlsli"

// マテリアル構造体（正しいメモリレイアウト）
struct Material
{
    float4x4 uvTransform; // 64バイト
    float4 color; // 16バイト
    int32_t enableLighting; // 4バイト
    float padding[3]; // 12バイト（16バイトアラインメント）
};

// ライト構造体群（LightManagerと互換性のある定義）
struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き
    float intensity; // 輝度
};

struct CameraForGPU
{
    float3 worldPosition; // カメラのワールド座標
    int32_t enableSpecular; // スペキュラー有効フラグ
    int32_t isHalfVector; // ハーフベクトル使用フラグ
};

struct PointLight
{
    float4 color; // 色
    float3 position; // 位置
    float intensity; // 輝度
    int32_t enablePointLight; // 有効フラグ
    float radius; // 最大距離
    float decay; // 減衰率
    float padding[2];
};

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
    int32_t enableSpotLight; // 有効フラグ
    float padding[2];
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 定数バッファ
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<CameraForGPU> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

// テクスチャとサンプラー
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

//=================================================================
// ライティング計算関数
//=================================================================

/// <summary>
/// 平行光源の計算
/// </summary>
float3 CalculateDirectionalLight(float3 worldPos, float3 normal, float3 toEye)
{
    float3 lightDir = normalize(-gDirectionalLight.direction);
    float diffuse = saturate(dot(normal, lightDir));
    
    // Diffuse
    float3 diffuseColor = gDirectionalLight.color.rgb * diffuse * gDirectionalLight.intensity;
    
    // Specular（有効な場合）
    float3 specularColor = float3(0, 0, 0);
    if (gCamera.enableSpecular)
    {
        float3 reflectLight;
        if (gCamera.isHalfVector)
        {
            // Half Vector方式
            float3 halfVector = normalize(lightDir + toEye);
            reflectLight = halfVector;
        }
        else
        {
            // Phong反射方式
            reflectLight = reflect(-lightDir, normal);
        }
        
        float specularPow = saturate(dot(reflectLight, normal));
        specularPow = pow(specularPow, 30.0f);
        specularColor = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity;
    }
    
    return diffuseColor + specularColor;
}

/// <summary>
/// ポイントライトの計算
/// </summary>
float3 CalculatePointLight(float3 worldPos, float3 normal, float3 toEye)
{
    if (!gPointLight.enablePointLight)
        return float3(0, 0, 0);
    
    float3 lightDir = gPointLight.position - worldPos;
    float distance = length(lightDir);
    
    // 距離減衰チェック
    if (distance > gPointLight.radius)
        return float3(0, 0, 0);
    
    lightDir = normalize(lightDir);
    float diffuse = saturate(dot(normal, lightDir));
    
    // 距離減衰
    float attenuation = pow(saturate(1.0f - (distance / gPointLight.radius)), gPointLight.decay);
    
    // Diffuse
    float3 diffuseColor = gPointLight.color.rgb * diffuse * gPointLight.intensity * attenuation;
    
    // Specular（有効な場合）
    float3 specularColor = float3(0, 0, 0);
    if (gCamera.enableSpecular)
    {
        float3 reflectLight;
        if (gCamera.isHalfVector)
        {
            float3 halfVector = normalize(lightDir + toEye);
            reflectLight = halfVector;
        }
        else
        {
            reflectLight = reflect(-lightDir, normal);
        }
        
        float specularPow = saturate(dot(reflectLight, normal));
        specularPow = pow(specularPow, 30.0f);
        specularColor = gPointLight.color.rgb * specularPow * gPointLight.intensity * attenuation;
    }
    
    return diffuseColor + specularColor;
}

/// <summary>
/// スポットライトの計算
/// </summary>
float3 CalculateSpotLight(float3 worldPos, float3 normal, float3 toEye)
{
    if (!gSpotLight.enableSpotLight)
        return float3(0, 0, 0);
    
    float3 lightDir = gSpotLight.position - worldPos;
    float distance = length(lightDir);
    
    // 距離チェック
    if (distance > gSpotLight.distance)
        return float3(0, 0, 0);
    
    lightDir = normalize(lightDir);
    
    // スポットライトの角度減衰
    float cosAngle = dot(lightDir, normalize(-gSpotLight.direction));
    float falloff = saturate((cosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
    
    if (falloff <= 0.0f)
        return float3(0, 0, 0);
    
    float diffuse = saturate(dot(normal, lightDir));
    
    // 距離減衰
    float attenuation = pow(saturate(1.0f - (distance / gSpotLight.distance)), gSpotLight.decay);
    
    // Diffuse
    float3 diffuseColor = gSpotLight.color.rgb * diffuse * gSpotLight.intensity * attenuation * falloff;
    
    // Specular（有効な場合）
    float3 specularColor = float3(0, 0, 0);
    if (gCamera.enableSpecular)
    {
        float3 reflectLight;
        if (gCamera.isHalfVector)
        {
            float3 halfVector = normalize(lightDir + toEye);
            reflectLight = halfVector;
        }
        else
        {
            reflectLight = reflect(-lightDir, normal);
        }
        
        float specularPow = saturate(dot(reflectLight, normal));
        specularPow = pow(specularPow, 30.0f);
        specularColor = gSpotLight.color.rgb * specularPow * gSpotLight.intensity * attenuation * falloff;
    }
    
    return diffuseColor + specularColor;
}

//=================================================================
// メインピクセルシェーダー
//=================================================================

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換してテクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 基本色
    float4 baseColor = gMaterial.color * textureColor * input.color;
    
    // アルファテスト
    if (baseColor.a < 0.01f)
    {
        discard;
    }
    
    // ライティングが無効な場合はそのまま出力
    if (gMaterial.enableLighting == 0)
    {
        output.color = baseColor;
        return output;
    }
    
    // ===== ライティング計算 =====
    
    // 法線を正規化
    float3 normal = normalize(input.normal);
    
    // ワールド座標
    float3 worldPos = input.worldPosition;
    
    // カメラへの方向ベクトル
    float3 toEye = normalize(gCamera.worldPosition - worldPos);
    
    // 環境光（アンビエント）
    float3 ambient = float3(0.1f, 0.1f, 0.1f);
    
    // 各ライトの影響を計算
    float3 lighting = ambient;
    lighting += CalculateDirectionalLight(worldPos, normal, toEye);
    lighting += CalculatePointLight(worldPos, normal, toEye);
    lighting += CalculateSpotLight(worldPos, normal, toEye);
    
    // 最終色（ライティングを適用）
    output.color = float4(baseColor.rgb * lighting, baseColor.a);
    
    return output;
}

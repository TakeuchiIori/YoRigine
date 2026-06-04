#include "Object3d.hlsli"
#include "../Light/Light.hlsli"
struct Material
{
    float4x4 uvTransform;
    float    stochasticStrength; // 0 = 通常タイル / 1 = タイル単位ランダムを最大適用
    float    _pad0;
    float    _pad1;
    float    _pad2;
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

struct MaterialConstant
{
    float3 Kd; // 拡散反射色（Materialから渡す）
};

// ディゾルブ用パラメータ。enabled = 0 のとき完全にコストゼロ（discard も lerp も走らない）
struct MaterialDissolve
{
    float  threshold;   // 0.0(全表示) ～ 1.0(完全消去)
    float  edgeWidth;   // threshold の上 edgeWidth ぶんがエッジ発光帯
    int    enabled;     // 0=無効
    float  noiseScale;  // ノイズの空間スケール
    float3 edgeColor;   // エッジ発光色
    float  _pad0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLightData> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLights> gPointLights : register(b4);
ConstantBuffer<SpotLights> gSpotLights : register(b5);
ConstantBuffer<MaterialColor> gMaterialColor : register(b6);
ConstantBuffer<MaterialLight> gMaterialLight : register(b7);
ConstantBuffer<MaterialConstant> gMaterialConstant : register(b8);
ConstantBuffer<MaterialDissolve> gMaterialDissolve : register(b9);


Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D gShadowMap : register(t2);

SamplerState gSampler : register(s0);
// シャドウマップ比較サンプラー
SamplerComparisonState gShadowSampler : register(s1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;

};

//-----------------------------------------------------------------------------
// タイル単位ハッシュランダム化サンプル
//   Inigo Quilez "Texture Repetition" の軽量版 (4-tap)。
//   各セル毎に乱数オフセットでサンプルし、bilinear で隣接 4 セルをブレンド。
//-----------------------------------------------------------------------------
float2 StochasticHash2(float2 p)
{
    p = float2(dot(p, float2(127.1f, 311.7f)),
               dot(p, float2(269.5f, 183.3f)));
    return frac(sin(p) * 43758.5453f);
}

// セルごとの「絵柄向き」を消すため、セル中心を軸に UV を回転させてからサンプルする
float4 SampleCellRotated(Texture2D tex, SamplerState sam, float2 uv, float2 cellId, float strength)
{
    float2 rnd = StochasticHash2(cellId);

    // 0..2π * strength のランダム回転（strength=0 で回転なし）
    float angle = rnd.x * 6.2831853f * strength;
    float ca = cos(angle), sa = sin(angle);

    // セル中心を原点に持ってきて回転
    float2 cellCenter = cellId + 0.5f;
    float2 local      = uv - cellCenter;
    float2 rotated    = float2(ca * local.x - sa * local.y,
                               sa * local.x + ca * local.y);

    // セル中心まわりに少しオフセット（strength で減衰）
    float2 offset = (rnd - 0.5f) * strength;

    return tex.Sample(sam, rotated + cellCenter + offset);
}

float4 SampleStochastic(Texture2D tex, SamplerState sam, float2 uv, float strength)
{
    // タイル格子 (uv が 1 進むごとに 1 セル)
    float2 iuv = floor(uv);
    float2 fuv = frac(uv);

    // 隣接 4 セルそれぞれを「セル毎の回転＋オフセット」でサンプル
    float4 ca = SampleCellRotated(tex, sam, uv, iuv + float2(0.0f, 0.0f), strength);
    float4 cb = SampleCellRotated(tex, sam, uv, iuv + float2(1.0f, 0.0f), strength);
    float4 cc = SampleCellRotated(tex, sam, uv, iuv + float2(0.0f, 1.0f), strength);
    float4 cd = SampleCellRotated(tex, sam, uv, iuv + float2(1.0f, 1.0f), strength);

    // bilinear で 4 セルをブレンドして接合部の段差を消す
    float2 b = smoothstep(0.0f, 1.0f, fuv);
    return lerp(lerp(ca, cb, b.x), lerp(cc, cd, b.x), b.y);
}

// プロシージャル 3D ハッシュ（テクスチャ不要のディゾルブマスク用）
float DissolveHash3D(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

// 補間付き 3D Value Noise（8 頂点のハッシュを smoothstep 補間）
float DissolveValueNoise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);

    float n000 = DissolveHash3D(i + float3(0, 0, 0));
    float n100 = DissolveHash3D(i + float3(1, 0, 0));
    float n010 = DissolveHash3D(i + float3(0, 1, 0));
    float n110 = DissolveHash3D(i + float3(1, 1, 0));
    float n001 = DissolveHash3D(i + float3(0, 0, 1));
    float n101 = DissolveHash3D(i + float3(1, 0, 1));
    float n011 = DissolveHash3D(i + float3(0, 1, 1));
    float n111 = DissolveHash3D(i + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, u.x);
    float nx10 = lerp(n010, n110, u.x);
    float nx01 = lerp(n001, n101, u.x);
    float nx11 = lerp(n011, n111, u.x);
    float nxy0 = lerp(nx00, nx10, u.y);
    float nxy1 = lerp(nx01, nx11, u.y);
    return lerp(nxy0, nxy1, u.z);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // === ディゾルブ：discard 判定 と エッジ判定フラグ ===
    float dissolveMask = 1.0f;
    bool  isInDissolveEdge = false;
    if (gMaterialDissolve.enabled != 0)
    {
        float scale = (gMaterialDissolve.noiseScale > 0.0001f) ? gMaterialDissolve.noiseScale : 1.0f;
        dissolveMask = DissolveValueNoise3D(input.worldPosition * scale);

        // threshold を下回ったピクセルは描画しない
        if (dissolveMask < gMaterialDissolve.threshold)
        {
            discard;
        }
        // エッジ帯（threshold ～ threshold + edgeWidth）にいるか
        isInDissolveEdge = dissolveMask < (gMaterialDissolve.threshold + gMaterialDissolve.edgeWidth);
    }

    // UV座標変換とテクスチャサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor;
    [branch] if (gMaterial.stochasticStrength > 0.001f)
    {
        textureColor = SampleStochastic(gTexture, gSampler, transformedUV.xy, gMaterial.stochasticStrength);
    }
    else
    {
        textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    }
    float4 baseColor = float4(gMaterialConstant.Kd, 1.0f) * textureColor;

    // 初期化
    float3 finalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 finalSpecular = float3(0.0f, 0.0f, 0.0f);

    if (gMaterialLight.enableLighting)
    {
        //===========================================================//
        //                   シャドウマップの計算
        //===========================================================//
        float3 proj = input.shadowPos.xyz / input.shadowPos.w;
        float2 shadowUV;
        shadowUV.x = proj.x * 0.5f + 0.5f;
        shadowUV.y = -proj.y * 0.5f + 0.5f; // Y軸反転

        // 主バイアスは PSO の SlopeScaledDepthBias 側で吸収する想定。
        // 残った D32_FLOAT 精度由来のチラつき対策に微小な定数だけ引く。
        float shadowDepth = proj.z - 0.0005f;

        float shadow = 1.0f; // デフォルトは影なし
        if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
        shadowDepth <= 1.0f)
        {
            // 3x3 PCF: HW 比較サンプラの 2x2 バイリニア × 9 タップでソフトシャドウ
            const float shadowMapSize = 4096.0f; // DsvManager::kShadowmapWidth と一致
            const float texel = 1.0f / shadowMapSize;
            float sum = 0.0f;
            [unroll]
            for (int sy = -1; sy <= 1; ++sy)
            {
                [unroll]
                for (int sx = -1; sx <= 1; ++sx)
                {
                    float2 off = float2(sx, sy) * texel;
                    sum += gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV + off, shadowDepth);
                }
            }
            shadow = sum / 9.0f;
        }

        // 影の濃さを調整
        float shadowFactor = max(shadow, 0.3f);
        
        
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
            float3 diffuseDirectional = gMaterialColor.color.rgb * baseColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
            // 鏡面反射 (Blinn-Phong)
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = max(dot(normalize(input.normal), halfVector), 0.0f);
            float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(saturate(NdotH), gMaterialLight.shininess);

            // フラグで有効化
            if (gMaterialLight.enableSpecular != 0)
            {
                finalSpecular += specularDirectional * shadowFactor;
            }
            finalDiffuse += diffuseDirectional * shadowFactor;
        }

        //===========================================================//
        
        //                      ポイントライトの計算          
        
        //===========================================================//
        
        for (int i = 0; i < min(gPointLights.numPointLights, MAX_LIGHT_COUNT); i++)
        {
            PointLightData pl = gPointLights.pointLights[i];
            // ライトが有効でない場合はスキップ
            if (!pl.isEnablePointLight)
                continue;
            // ライト方向ベクトル
            float3 pointLightDirection = normalize(pl.position - input.worldPosition);
            
            float distance = length(pl.position - input.worldPosition); // ポイントライトへの距離
            float factor = pow(saturate(-distance / pl.radius + 1.0f), pl.decay); //指数によるコントロール
            
            // 拡散反射
            float NdotLPoint = max(dot(normalize(input.normal), pointLightDirection), 0.0f);
            float3 diffusePoint = gMaterialColor.color.rgb * baseColor.rgb * pl.color.rgb * NdotLPoint * pl.intensity * factor;
            // 鏡面反射 (Blinn-Phong)
            float3 halfVectorPoint = normalize(pointLightDirection + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = pl.color.rgb * pl.intensity * pow(saturate(NdotHPoint), gMaterialLight.shininess) * factor;
        
            // フラグで有効化
            if (gMaterialLight.enableSpecular != 0)
            {
                finalSpecular += specularPoint * shadowFactor;
            }
            finalDiffuse += diffusePoint * shadowFactor;
        }
        
        //===========================================================//
        
        //                      スポットライトの計算    
        
        //===========================================================//
        
        for (int j = 0; j < min(gSpotLights.numSpotLights, MAX_LIGHT_COUNT); j++)
        {
            SpotLightData sl = gSpotLights.spotLights[j];
            // ライトが有効でない場合はスキップ
            if(!sl.isEnableSpotLight) continue;
            
            // スポットライトの方向ベクトル
            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - sl.position);
            // ライトとの距離
            float distanceToSurface = length(sl.position - input.worldPosition);
            // 距離による減衰率 (逆距離の減衰計算)
            float distanceDecay = pow(saturate(-distanceToSurface / sl.distance + 1.0f), sl.decay);
            // スポットライトの角度による減衰 (Falloff開始角度を考慮)
            float cosAngle = dot(spotLightDirectionOnSurface, sl.direction);
            float angleFalloff = saturate((cosAngle - sl.cosFalloffStart) / (sl.cosAngle - sl.cosFalloffStart));
            // 総減衰係数
            float falloffFactor = angleFalloff * distanceDecay;

            // 拡散反射 (NdotL)
            float NdotLPoint = max(dot(normalize(input.normal), -spotLightDirectionOnSurface), 0.0f);
            float3 diffusePoint = gMaterialColor.color.rgb * baseColor.rgb * sl.color.rgb * NdotLPoint * sl.intensity * falloffFactor;

            // 鏡面反射 (Blinn-Phong)
            float3 halfVectorPoint = normalize(-spotLightDirectionOnSurface + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = sl.color.rgb * sl.intensity * pow(saturate(NdotHPoint), gMaterialLight.shininess) * falloffFactor;

            // スペキュラー反射の有効化
            if (gMaterialLight.enableSpecular != 0)
            {
                finalSpecular += specularPoint * shadowFactor;
            }
            finalDiffuse += diffusePoint * shadowFactor;
        }

        
        //===========================================================//
        
        //                     環境マップの計算
        
        //===========================================================//
        
        float3 environmentColor = float3(0.0f, 0.0f, 0.0f);
        if (gMaterialLight.enableEnvironment)
        {
            float3 incident = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectionVector = reflect(incident, normalize(input.normal));
            environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionVector).rgb * gMaterialLight.environmentCoeffcient;
        }

        //===========================================================//
        
        //                     最終的な色を合成  
        //===========================================================//
        output.color.rgb = finalDiffuse + finalSpecular + environmentColor;
    }
    else
    {
        
        //===========================================================//
        
        //                    ライティングなしの場合
        
        //===========================================================//

        output.color.rgb = gMaterialColor.color.rgb * baseColor.rgb;
    }

    
    // アルファ値の設定
    output.color.a = gMaterialColor.color.a * baseColor.a;

    // === ディゾルブのエッジ発光（discard を生き残ったピクセルのうちエッジ帯のみ） ===
    if (gMaterialDissolve.enabled != 0 && isInDissolveEdge)
    {
        // しきい値直上 = 0、エッジ帯の外側 = 1 となる正規化
        float t = saturate((dissolveMask - gMaterialDissolve.threshold) /
                           max(gMaterialDissolve.edgeWidth, 0.0001f));
        // しきい値に近いほど強く発光色を乗せる
        float edgeStrength = 1.0f - t;
        output.color.rgb = lerp(output.color.rgb, gMaterialDissolve.edgeColor, edgeStrength);
    }

    return output;
}

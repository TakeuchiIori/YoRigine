#include "Object3dInstanced.hlsli"
#include "../Light/Light.hlsli"
#include "../Shadow/Cascade.hlsli"

// Per-instance データ (VS と完全一致)
struct InstanceData
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4   color;
    float4x4 uvTransform;
    float    stochasticStrength;
    float    _pad0;
    float    _pad1;
    float    _pad2;
};

struct MaterialLight
{
    int   enableLighting;
    int   enableSpecular;
    int   enableEnvironment;
    int   isHalfVector;
    float shininess;
    float environmentCoeffcient;
    // --- トゥーン拡張（C++ MaterialLighting::MaterialLight と一致） ---
    int   enableToon;
    int   toonBands;
    int   enableRim;
    int   enableToonSpecular;
    float rimPower;
    float rimIntensity;
    float toonSpecularThreshold;
    float toonEdgeSoftness;
    // --- 2D表現（アニメ塗り） ---
    float  shadowStrength;
    float  _pad0;
    float3 shadowColor;
    float  _pad1;
    float3 highlightColor;
    float  _pad2;
};

struct MaterialConstant
{
    float3 Kd;
};

StructuredBuffer<InstanceData> gInstances : register(t3);

ConstantBuffer<CascadeShadow> gCascadeShadow : register(b1);
ConstantBuffer<DirectionalLightData> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLights> gPointLights : register(b4);
ConstantBuffer<SpotLights> gSpotLights : register(b5);
ConstantBuffer<MaterialLight> gMaterialLight : register(b7);
ConstantBuffer<MaterialConstant> gMaterialConstant : register(b8);

Texture2D<float4>   gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2DArray<float> gShadowMap : register(t2);

SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1; // G-buffer: ワールド空間法線(.xyz) + 書き込みマスク(.w)
};

// 拡散項を階調化する（Object3D.PS と同仕様）。
float ToonQuantize(float x)
{
    if (gMaterialLight.enableToon == 0) return x;
    float bands = max((float) gMaterialLight.toonBands, 1.0f);
    x = saturate(x);
    float s = x * bands;
    float lower = floor(s);
    float f = s - lower;
    float soft = saturate(gMaterialLight.toonEdgeSoftness * bands);
    float blend = (soft > 1e-4f) ? smoothstep(1.0f - soft, 1.0f, f) : 0.0f;
    return (lower + blend) / bands;
}

float ToonSpecular(float specValue)
{
    if (gMaterialLight.enableToonSpecular == 0) return specValue;
    return step(gMaterialLight.toonSpecularThreshold, specValue);
}

// タイル単位ハッシュランダム化サンプル (Object3d.PS と同じ)
float2 StochasticHash2(float2 p)
{
    p = float2(dot(p, float2(127.1f, 311.7f)),
               dot(p, float2(269.5f, 183.3f)));
    return frac(sin(p) * 43758.5453f);
}

float4 SampleStochastic(Texture2D tex, SamplerState sam, float2 uv, float strength)
{
    float2 iuv = floor(uv);
    float2 fuv = frac(uv);

    float2 oa = (StochasticHash2(iuv + float2(0.0f, 0.0f)) - 0.5f) * strength;
    float2 ob = (StochasticHash2(iuv + float2(1.0f, 0.0f)) - 0.5f) * strength;
    float2 oc = (StochasticHash2(iuv + float2(0.0f, 1.0f)) - 0.5f) * strength;
    float2 od = (StochasticHash2(iuv + float2(1.0f, 1.0f)) - 0.5f) * strength;

    float4 ca = tex.Sample(sam, uv + oa);
    float4 cb = tex.Sample(sam, uv + ob);
    float4 cc = tex.Sample(sam, uv + oc);
    float4 cd = tex.Sample(sam, uv + od);

    float2 b = smoothstep(0.0f, 1.0f, fuv);
    return lerp(lerp(ca, cb, b.x), lerp(cc, cd, b.x), b.y);
}

PixelShaderOutput main(InstancedVertexShaderOutput input)
{
    PixelShaderOutput output;

    // G-buffer 法線出力（ワールド空間）
    output.normal = float4(normalize(input.normal), 1.0f);

    InstanceData inst = gInstances[input.instanceID];

    // UV変換 + テクスチャサンプリング (per-instance uvTransform / stochasticStrength)
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), inst.uvTransform);
    float4 textureColor;
    [branch] if (inst.stochasticStrength > 0.001f)
    {
        textureColor = SampleStochastic(gTexture, gSampler, transformedUV.xy, inst.stochasticStrength);
    }
    else
    {
        textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    }
    float4 baseColor = float4(gMaterialConstant.Kd, 1.0f) * textureColor;

    float3 finalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 finalSpecular = float3(0.0f, 0.0f, 0.0f);

    if (gMaterialLight.enableLighting)
    {
        // シャドウマップ（カスケード）：worldPosition から最も近いカスケードを選んで PCF
        const float shadowMapSize = 2048.0f;
        float shadow = SampleCascadeShadow(gShadowMap, gShadowSampler, gCascadeShadow,
                                           input.worldPosition, shadowMapSize);
        // 落ち影。トゥーン有効時はくっきり2値化(ベタ影)、無効時は従来のソフト(PCF)を維持。
        float shadowFactor = (gMaterialLight.enableToon != 0) ? step(0.5f, shadow) : max(shadow, 0.3f);

        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // ディレクショナルライト
        if (gDirectionalLight.isEnableDirectionalLighting)
        {
            float3 albedo = inst.color.rgb * baseColor.rgb;
            float NdotL = max(dot(normalize(input.normal), -gDirectionalLight.direction), 0.0f);
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = max(dot(normalize(input.normal), halfVector), 0.0f);

            if (gMaterialLight.enableToon != 0)
            {
                // 回り込み(ハーフランバート pow)は明暗境界を濁らせるため使わず、
                // 素のランバート × 落ち影 を階調化。落ち影を「受光量」として畳み込むことで、
                // キャストシャドウも shadowColor のベタ塗りになり境界がパキッと出る。
                float lambert = saturate(NdotL) * shadowFactor;
                float df = ToonQuantize(lambert);
                float3 effShadow = lerp(float3(1.0f, 1.0f, 1.0f), gMaterialLight.shadowColor, gMaterialLight.shadowStrength);
                float3 shade = lerp(effShadow, float3(1.0f, 1.0f, 1.0f), df);
                finalDiffuse += albedo * gDirectionalLight.color.rgb * gDirectionalLight.intensity * shade;

                if (gMaterialLight.enableToonSpecular != 0)
                {
                    float blob = ToonSpecular(pow(saturate(NdotH), gMaterialLight.shininess));
                    finalSpecular += blob * gMaterialLight.highlightColor * gDirectionalLight.intensity;
                }
            }
            else
            {
                float cosV = pow(NdotL * 0.5f + 0.5f, 2.0f);
                float3 diffuseDirectional = albedo * gDirectionalLight.color.rgb * cosV * gDirectionalLight.intensity;
                float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(saturate(NdotH), gMaterialLight.shininess);
                if (gMaterialLight.enableSpecular != 0)
                    finalSpecular += specularDirectional * shadowFactor;
                finalDiffuse += diffuseDirectional * shadowFactor;
            }
        }

        // ポイントライト
        for (int i = 0; i < min(gPointLights.numPointLights, MAX_LIGHT_COUNT); i++)
        {
            PointLightData pl = gPointLights.pointLights[i];
            if (!pl.isEnablePointLight) continue;
            float3 pointLightDirection = normalize(pl.position - input.worldPosition);
            float distance = length(pl.position - input.worldPosition);
            float factor = pow(saturate(-distance / pl.radius + 1.0f), pl.decay);

            float NdotLPoint = max(dot(normalize(input.normal), pointLightDirection), 0.0f);
            NdotLPoint = ToonQuantize(NdotLPoint);
            float3 diffusePoint = inst.color.rgb * baseColor.rgb * pl.color.rgb * NdotLPoint * pl.intensity * factor;
            float3 halfVectorPoint = normalize(pointLightDirection + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = pl.color.rgb * pl.intensity * ToonSpecular(pow(saturate(NdotHPoint), gMaterialLight.shininess)) * factor;

            if (gMaterialLight.enableSpecular != 0)
                finalSpecular += specularPoint;
            finalDiffuse += diffusePoint;
        }

        // スポットライト
        for (int j = 0; j < min(gSpotLights.numSpotLights, MAX_LIGHT_COUNT); j++)
        {
            SpotLightData sl = gSpotLights.spotLights[j];
            if (!sl.isEnableSpotLight) continue;
            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - sl.position);
            float distanceToSurface = length(sl.position - input.worldPosition);
            float distanceDecay = pow(saturate(-distanceToSurface / sl.distance + 1.0f), sl.decay);
            float cosAngle = dot(spotLightDirectionOnSurface, sl.direction);
            float angleFalloff = saturate((cosAngle - sl.cosFalloffStart) / (sl.cosAngle - sl.cosFalloffStart));
            float falloffFactor = angleFalloff * distanceDecay;

            float NdotLPoint = max(dot(normalize(input.normal), -spotLightDirectionOnSurface), 0.0f);
            NdotLPoint = ToonQuantize(NdotLPoint);
            float3 diffusePoint = inst.color.rgb * baseColor.rgb * sl.color.rgb * NdotLPoint * sl.intensity * falloffFactor;
            float3 halfVectorPoint = normalize(-spotLightDirectionOnSurface + toEye);
            float NdotHPoint = max(dot(normalize(input.normal), halfVectorPoint), 0.0f);
            float3 specularPoint = sl.color.rgb * sl.intensity * ToonSpecular(pow(saturate(NdotHPoint), gMaterialLight.shininess)) * falloffFactor;

            if (gMaterialLight.enableSpecular != 0)
                finalSpecular += specularPoint;
            finalDiffuse += diffusePoint;
        }

        // 環境マップ
        float3 environmentColor = float3(0.0f, 0.0f, 0.0f);
        if (gMaterialLight.enableEnvironment)
        {
            float3 incident = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectionVector = reflect(incident, normalize(input.normal));
            environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionVector).rgb * gMaterialLight.environmentCoeffcient;
        }

        // リムライト（トゥーン用の逆光フチ）
        float3 rim = float3(0.0f, 0.0f, 0.0f);
        if (gMaterialLight.enableRim != 0)
        {
            float rimFactor = pow(1.0f - saturate(dot(normalize(input.normal), toEye)), gMaterialLight.rimPower);
            rim = rimFactor * gMaterialLight.rimIntensity * gDirectionalLight.color.rgb;
        }

        output.color.rgb = finalDiffuse + finalSpecular + environmentColor + rim;
    }
    else
    {
        output.color.rgb = inst.color.rgb * baseColor.rgb;
    }

    output.color.a = inst.color.a * baseColor.a;
    return output;
}

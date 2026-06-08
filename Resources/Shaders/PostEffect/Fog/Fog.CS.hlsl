#include "../Common/PostEffectCS.hlsli"

struct FogParams
{
    float4x4 viewProjectionInverse;

    float3 cameraPos;
    float  fogDensity;

    float3 fogColor;
    float  fogStart;

    float3 sunDirection;
    float  sunInscatterStrength;

    float3 sunColor;
    float  skyFogClamp;

    float heightFogTop;
    float heightFogBottom;
    float heightFogDensity;
    float heightFogDistanceScale;
};

Texture2D<float4> gInput : register(t0);
Texture2D<float>  gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<FogParams> gFog : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float4 sceneColor = gInput.SampleLevel(gSampler, uv, 0);
    float ndcDepth = gDepthTexture.SampleLevel(gSamplerPoint, uv, 0);

    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clipPos = float4(ndcXY, ndcDepth, 1.0f);
    float4 worldPos4 = mul(clipPos, gFog.viewProjectionInverse);
    float3 worldPos = worldPos4.xyz / worldPos4.w;

    float3 toPixel = worldPos - gFog.cameraPos;
    float distance = length(toPixel);
    float3 viewDir = toPixel / max(distance, 1e-4f);

    bool isSky = ndcDepth >= 0.9999f;

    float distFromStart = max(0.0f, distance - gFog.fogStart);
    float distFog = 1.0f - exp(-gFog.fogDensity * distFromStart);

    float hVal = saturate((gFog.heightFogTop - worldPos.y)
                       / max(gFog.heightFogTop - gFog.heightFogBottom, 0.001f));
    float heightDensity = hVal * hVal * gFog.heightFogDensity;
    float heightFog = 1.0f - exp(-heightDensity * distance * gFog.heightFogDistanceScale);

    float fogAmount = saturate(distFog + heightFog - distFog * heightFog);

    if (isSky)
    {
        fogAmount = min(fogAmount, gFog.skyFogClamp);
    }

    float cosSun = saturate(dot(viewDir, -gFog.sunDirection));
    float sunPower = pow(cosSun, 8.0f);
    float3 inscatter = gFog.sunColor * (sunPower * gFog.sunInscatterStrength);
    float3 fogColorFinal = gFog.fogColor + inscatter;

    float3 result = lerp(sceneColor.rgb, fogColorFinal, fogAmount);
    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, sceneColor.a);
}

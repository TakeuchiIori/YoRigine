#include "../Common/PostEffectCS.hlsli"

struct GodRaysParams
{
    float2 sunUV;
    float  sunVisibility;
    float  density;

    float3 sunColor;
    float  weight;

    float decay;
    float exposure;
    int   numSamples;
    float skyThreshold;
};

Texture2D<float4> gInput : register(t0);
Texture2D<float>  gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<GodRaysParams> gGodRays : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float4 sceneColor = gInput.SampleLevel(gSampler, uv, 0);

    if (gGodRays.sunVisibility <= 0.001f)
    {
        float3 c = LinearToSRGB(sceneColor.rgb);
        gOutput[dtid.xy] = float4(c, sceneColor.a);
        return;
    }

    int n = max(gGodRays.numSamples, 1);
    float2 deltaTexCoord = (uv - gGodRays.sunUV) * (gGodRays.density / float(n));

    float2 texCoord = uv;
    float illumination = 0.0f;
    float currentWeight = gGodRays.weight;

    [unroll(64)]
    for (int i = 0; i < 64; ++i)
    {
        if (i >= n) break;

        texCoord -= deltaTexCoord;
        float2 sampleUV = saturate(texCoord);

        float depth = gDepthTexture.SampleLevel(gSamplerPoint, sampleUV, 0);
        float skyMask = (depth >= gGodRays.skyThreshold) ? 1.0f : 0.0f;

        illumination += skyMask * currentWeight;
        currentWeight *= gGodRays.decay;
    }

    illumination *= gGodRays.exposure * gGodRays.sunVisibility;
    float3 godRays = illumination * gGodRays.sunColor;
    float3 result = sceneColor.rgb + godRays;

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, sceneColor.a);
}

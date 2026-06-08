#include "../Common/PostEffectCS.hlsli"

cbuffer cbPostEffect : register(b0)
{
    float progress;
    float2 resolution;
    float time;
};

Texture2D<float4> sceneTex : register(t0);
Texture2D<float4> crackTex : register(t1);
SamplerState smp : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898f, 78.233f))) * 43758.5453123f);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float crack = crackTex.SampleLevel(smp, uv, 0).r;
    float fragmentID = floor(crack * 40.0f) / 40.0f;

    float2 seed = float2(fragmentID * 12.34f, fragmentID * 56.78f);
    float randX = random(seed) * 2.0f - 1.0f;
    float randY = random(seed + 0.5f) * 2.0f - 1.0f;
    float randRot = random(seed + 1.0f) * 6.28f;
    float randSpeed = 0.7f + random(seed + 2.0f) * 0.6f;

    float t = 0.0f;
    float darkness = 0.0f;

    if (progress < 0.4f)
    {
        t = progress / 0.4f;
        t = t * t;
        darkness = t * 0.5f;
    }
    else if (progress < 0.6f)
    {
        t = 1.0f;
        float darkProgress = (progress - 0.4f) / 0.2f;
        darkness = 0.5f + darkProgress * 0.5f;
    }
    else
    {
        float fadeProgress = (progress - 0.6f) / 0.4f;
        t = 1.0f - fadeProgress;
        darkness = 1.0f - fadeProgress;
    }

    float2 center = float2(0.5f, 0.5f);
    float2 toEdge = uv - center;
    float2 direction = normalize(toEdge + float2(randX, randY) * 0.3f);

    float moveAmount = t * randSpeed * 0.5f;
    float2 displacement = direction * moveAmount;
    displacement.y += t * t * 0.3f;

    float2 uvCentered = uv - center;
    float angle = randRot * t;
    float ca = cos(angle);
    float sa = sin(angle);
    float2 rotatedUV = float2(
        uvCentered.x * ca - uvCentered.y * sa,
        uvCentered.x * sa + uvCentered.y * ca
    ) + center;

    float2 finalUV = rotatedUV + displacement;
    float4 color = sceneTex.SampleLevel(smp, finalUV, 0);

    float inBounds = (finalUV.x >= 0.0f && finalUV.x <= 1.0f &&
                     finalUV.y >= 0.0f && finalUV.y <= 1.0f) ? 1.0f : 0.0f;

    float2 pixelSize = 1.0f / resolution;
    float diff = abs(crack - crackTex.SampleLevel(smp, uv + float2(pixelSize.x, 0), 0).r)
               + abs(crack - crackTex.SampleLevel(smp, uv + float2(0, pixelSize.y), 0).r);
    float isEdge = smoothstep(0.03f, 0.06f, diff);

    if (progress < 0.6f)
    {
        color.rgb = lerp(color.rgb, float3(0, 0, 0), isEdge * min(t, 1.0f));
    }

    color.rgb *= inBounds;
    color.rgb *= (1.0f - darkness);

    color.rgb = LinearToSRGB(color.rgb);
    gOutput[dtid.xy] = color;
}

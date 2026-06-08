#include "../Common/PostEffectCS.hlsli"

cbuffer HalftoneParams : register(b0)
{
    float dotSize;
    float angle;
    float strength;
    float threshold;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

static const float PI = 3.14159265f;

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float2 screenPos = uv * float2(w, h);

    float4 original = gInput.SampleLevel(gSampler, uv, 0);
    float lum = Luminance(original.rgb);

    float rad = angle * PI / 180.0f;
    float cosA = cos(rad);
    float sinA = sin(rad);

    float2 rotPos = float2(
        cosA * screenPos.x - sinA * screenPos.y,
        sinA * screenPos.x + cosA * screenPos.y
    );

    float2 cellCoord = frac(rotPos / dotSize) - 0.5f;
    float dist = length(cellCoord);

    float2 cellOriginRot = (floor(rotPos / dotSize) + 0.5f) * dotSize;
    float2 cellOriginWorld = float2(
        cosA * cellOriginRot.x + sinA * cellOriginRot.y,
        -sinA * cellOriginRot.x + cosA * cellOriginRot.y
    );
    float2 cellUV = saturate(cellOriginWorld / float2(w, h));
    float cellLum = Luminance(gInput.SampleLevel(gSampler, cellUV, 0).rgb);

    float darkness = saturate(1.0f - cellLum);
    float dotRadius = darkness * 0.5f * (dotSize / (dotSize + 1.0f));

    float inDot = step(dist, dotRadius);
    float applyMask = step(lum, threshold);

    float3 dotColor = original.rgb * (1.0f - inDot * 0.85f);
    float3 result = lerp(original.rgb, dotColor, applyMask * strength);

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, original.a);
}

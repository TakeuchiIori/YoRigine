#include "../Common/PostEffectCS.hlsli"

cbuffer CrossHatchParams : register(b0)
{
    float lineSpacing;
    float lineWidth;
    float strength;
    float padding;
};

Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float HatchLine(float2 pixel, float angle, float spacing, float width)
{
    float cosA = cos(angle);
    float sinA = sin(angle);
    float proj = cosA * pixel.x + sinA * pixel.y;
    float t = frac(proj / spacing);
    float lineHalf = (width * 0.5f) / spacing;
    return step(t, lineHalf) + step(1.0f - lineHalf, t);
}

static const float PI = 3.14159265f;

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float2 pixel = uv * float2(w, h);

    float4 original = gInput.SampleLevel(gSampler, uv, 0);
    float lum = Luminance(original.rgb);

    float hatch = 0.0f;
    if (lum < 0.75f) hatch = max(hatch, HatchLine(pixel, PI * 0.25f, lineSpacing, lineWidth));
    if (lum < 0.55f) hatch = max(hatch, HatchLine(pixel, PI * 0.75f, lineSpacing, lineWidth));
    if (lum < 0.35f) hatch = max(hatch, HatchLine(pixel, 0.0f,        lineSpacing, lineWidth));
    if (lum < 0.15f) hatch = max(hatch, HatchLine(pixel, PI * 0.5f,   lineSpacing, lineWidth));

    float3 hatchColor = original.rgb * 0.15f;
    float3 result = lerp(original.rgb, hatchColor, hatch * strength);

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, original.a);
}

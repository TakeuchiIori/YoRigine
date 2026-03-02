Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ExposureBuffer : register(b0)
{
    float exposure;
};

float3 ToneMapACES(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 hdr = gTexture.Sample(gSampler, uv).rgb;
    hdr *= exposure;

    float3 mapped = ToneMapACES(hdr);
    mapped = pow(mapped, 1.0 / 2.2); // ガンマ補正

    return float4(mapped, 1.0);
}

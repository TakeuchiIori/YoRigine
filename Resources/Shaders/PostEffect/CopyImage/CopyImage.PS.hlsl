#include "../FullScreen./FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ACES フィルミックトーンマップ（近似）。HDR リニア → [0,1] リニア。
float3 ToneMapACES(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// パイプライン最終段の blit。入力は HDR リニア(OffScreen / 中間バッファ = R16F)。
// ここでトーンマップして [0,1] リニアを出力し、sRGB バックバッファRTVが
// ガンマエンコードする（＝ガンマは一元的にここだけ）。
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 hdr = gTexture.Sample(gSampler, input.texCoord);
    output.color = float4(ToneMapACES(hdr.rgb), hdr.a);
    return output;
}
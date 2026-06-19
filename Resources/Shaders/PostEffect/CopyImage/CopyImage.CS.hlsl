#include "../Common/PostEffectCS.hlsli"

// SRGB ビュー越しに読むので Sample 戻り値は線形値
Texture2D<float4> gInput : register(t0);
SamplerState gSampler : register(s0);

// UNORM ビュー (SRGB自動変換なし) なので、書き込み前に LinearToSRGB を適用する
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h)
        return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);
    float4 c = gInput.SampleLevel(gSampler, uv, 0); // linear

    c.rgb = LinearToSRGB(c.rgb); // UAV(UNORM)へは手動エンコード
    gOutput[dtid.xy] = c;
}

#include "../Common/PostEffectCS.hlsli"

// 法線 G-buffer をそのまま色として可視化するデバッグ用ポストエフェクト。
// G-buffer にはワールド空間法線(.xyz) と書き込みマスク(.w) が入っている。
// 法線が書かれていない画素(背景/半透明)は .w==0 なので黒で表示する。

Texture2D<float4> gInput         : register(t0); // シーンカラー(未使用だが RS 共有のため宣言)
Texture2D<float>  gDepthTexture  : register(t1); // 深度(未使用)
Texture2D<float4> gNormalTexture : register(t2); // 法線 G-buffer
SamplerState gSampler      : register(s0);
SamplerState gSamplerPoint : register(s1);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float4 n = gNormalTexture.SampleLevel(gSamplerPoint, uv, 0);

    // .w==0 は法線未書き込み（背景など）→ 黒
    float3 col = (n.w > 0.0f) ? (normalize(n.xyz) * 0.5f + 0.5f) : float3(0.0f, 0.0f, 0.0f);

    gOutput[dtid.xy] = float4(LinearToSRGB(col), 1.0f);
}

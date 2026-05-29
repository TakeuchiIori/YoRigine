#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParams : register(b0)
{
    float threshold;          // 輝度しきい値 (0.0 ~ 1.0, default: 0.6)
    float intensity;          // ブルームの強さ (0.0 ~ 3.0, default: 0.5)
    float spread;             // サンプリング半径 [texels] (1.0 ~ 20.0, default: 6.0)
    float colorTemperature;   // 暖色(+1.0) / 寒色(-1.0) (default: 0.3)
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// しきい値を超えた輝度部分だけを抽出する
float3 ExtractBright(float3 c)
{
    float lum = Luminance(c);
    float excess = max(0.0, lum - threshold);
    // ソフトニー: 急に切れないように滑らかに
    float factor = excess / max(lum, 0.0001);
    return c * factor;
}

// 暖色/寒色のカラートント
float3 ApplyColorTemp(float3 bloom, float temp)
{
    bloom.r = saturate(bloom.r + temp * 0.18);
    bloom.g = saturate(bloom.g + temp * 0.04);
    bloom.b = saturate(bloom.b - temp * 0.12);
    return bloom;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = float2(rcp(float(width)), rcp(float(height)));

    float4 original = gTexture.Sample(gSampler, input.texCoord);

    // ---- 2リング × 8方向 + 中心 = 17サンプルのブルーム ----
    // 内リング (半径 spread)
    const float PI2_8 = 3.14159265 * 2.0 / 8.0;
    float3 bloom = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float angle = PI2_8 * float(i);
        float2 uv1 = input.texCoord + float2(cos(angle), sin(angle)) * texelSize * spread;
        float2 uv2 = input.texCoord + float2(cos(angle), sin(angle)) * texelSize * spread * 2.0;

        float3 s1 = gTexture.Sample(gSampler, uv1).rgb;
        float3 s2 = gTexture.Sample(gSampler, uv2).rgb;

        bloom += ExtractBright(s1) * 0.6;
        bloom += ExtractBright(s2) * 0.3;
        totalWeight += 0.9;
    }

    // 中心点も加味
    bloom += ExtractBright(original.rgb) * 0.5;
    totalWeight += 0.5;

    bloom /= totalWeight;

    // カラー温度を適用
    bloom = ApplyColorTemp(bloom, colorTemperature);

    // 元画像にブルームを加算合成
    output.color = float4(saturate(original.rgb + bloom * intensity), original.a);

    return output;
}

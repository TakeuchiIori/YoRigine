#include "../FullScreen./FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ChromaticParams : register(b0)
{
    float aberrationStrength; // ズレの最大値（例：0.005）
    float2 screenSize; // 解像度
    float edgeStrength; // 中心からの距離に応じた補正（0〜1）
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 中心からの距離計算
    float2 center = float2(0.5f, 0.5f);
    float2 dir = input.texCoord - center;
    float dist = length(dir);
    
    // 画面端に向かって効果を強くする
    float falloff = smoothstep(0.0, 0.7, dist);
    
    // より大きな値でテスト（0.01〜0.02程度）
    float aberration = aberrationStrength * falloff;
    
    // 方向ベクトルを正規化
    float2 uvOffset = normalize(dir) * aberration;
    
    // RGBチャンネルを別々のオフセットでサンプリング
    float r = gTexture.Sample(gSampler, input.texCoord - uvOffset).r;
    float g = gTexture.Sample(gSampler, input.texCoord).g;
    float b = gTexture.Sample(gSampler, input.texCoord + uvOffset).b;
    
    output.color = float4(r, g, b, 1.0);
    
    // デバッグ：効果が強すぎる場合の確認用
    // output.color = float4(dist, 0, 0, 1); // 距離を赤で可視化
    
    return output;
}
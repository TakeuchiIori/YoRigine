#include "../FullScreen/FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer HalftoneParams : register(b0)
{
    float dotSize;     // ドットのセルサイズ [pixels] (2.0 ~ 16.0, default: 6.0)
    float angle;       // グリッド回転角 [degrees] (default: 45.0)
    float strength;    // エフェクト強度 (0.0 ~ 1.0, default: 0.85)
    float threshold;   // この輝度以下の領域にのみドットを表示 (0.0 ~ 1.0, default: 0.75)
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

static const float PI = 3.14159265;

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 screenPos = input.texCoord * float2(width, height);

    float4 original = gTexture.Sample(gSampler, input.texCoord);
    float lum = Luminance(original.rgb);

    // ---- グリッド座標計算 ----
    float rad = angle * PI / 180.0;
    float cosA = cos(rad);
    float sinA = sin(rad);

    // 回転したグリッド空間へ変換
    float2 rotPos = float2(
        cosA * screenPos.x - sinA * screenPos.y,
        sinA * screenPos.x + cosA * screenPos.y
    );

    // セル内の相対座標 (-0.5 ~ 0.5)
    float2 cellCoord = frac(rotPos / dotSize) - 0.5;
    float dist = length(cellCoord);

    // セル中心での輝度をサンプリング（ドット径の決定用）
    float2 cellOriginRot = (floor(rotPos / dotSize) + 0.5) * dotSize;
    float2 cellOriginWorld = float2(
        cosA * cellOriginRot.x + sinA * cellOriginRot.y,
        -sinA * cellOriginRot.x + cosA * cellOriginRot.y
    );
    float2 cellUV = cellOriginWorld / float2(width, height);
    cellUV = saturate(cellUV);
    float cellLum = Luminance(gTexture.Sample(gSampler, cellUV).rgb);

    // 暗い部分ほどドットが大きくなる
    float darkness = saturate(1.0 - cellLum);
    float dotRadius = darkness * 0.5 * (dotSize / (dotSize + 1.0));

    // ドット内かどうか
    float inDot = step(dist, dotRadius);

    // 輝度しきい値以下の領域にのみ適用
    float applyMask = step(lum, threshold);

    // ドット部分は元の色を暗くする（黒ドット）
    float3 dotColor = original.rgb * (1.0 - inDot * 0.85);

    output.color = float4(lerp(original.rgb, dotColor, applyMask * strength), original.a);

    return output;
}

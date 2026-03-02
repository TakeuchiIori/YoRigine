#include "../FullScreen./FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 基本色調整パラメータ
cbuffer ColorAdjustParams : register(b0)
{
    float brightness;       // 明度 (-1.0 ~ 1.0)
    float contrast;         // コントラスト (0.0 ~ 2.0, 1.0が標準)
    float saturation;       // 彩度 (0.0 ~ 2.0, 1.0が標準)
    float hue;              // 色相 (-180.0 ~ 180.0 度)
};

// ガンマ・露出調整パラメータ
cbuffer ToneParams : register(b1)
{
    float gamma;            // ガンマ値 (0.1 ~ 3.0, 2.2が標準)
    float exposure;         // 露出 (-3.0 ~ 3.0)
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// RGB to HSV 変換
float3 RGBtoHSV(float3 rgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB 変換
float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

// 基本色調整
float3 ApplyColorAdjust(float3 color)
{
    // 明度調整
    color += brightness;
    
    // コントラスト調整
    color = (color - 0.5) * contrast + 0.5;
    
    // 彩度と色相調整のためにHSVに変換
    float3 hsv = RGBtoHSV(color);
    
    // 色相調整
    hsv.x += hue / 360.0;
    hsv.x = frac(hsv.x + 1.0);
    
    // 彩度調整
    hsv.y *= saturation;
    hsv.y = clamp(hsv.y, 0.0, 1.0);
    
    // RGBに戻す
    return HSVtoRGB(hsv);
}

// 露出とガンマ調整
float3 ApplyToneAdjust(float3 color)
{
    // 露出調整
    color *= pow(2.0, exposure);
    
    // ガンマ補正
    color = pow(abs(color), 1.0 / gamma);
    
    return color;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャサンプリング
    float4 originalColor = gTexture.Sample(gSampler, input.texCoord);
    
    // 色調整適用
    float3 adjustedColor = ApplyColorAdjust(originalColor.rgb);
    
    // トーン調整適用
    adjustedColor = ApplyToneAdjust(adjustedColor);
    
    // 最終クランプ
    adjustedColor = clamp(adjustedColor, 0.0, 1.0);
    
    // アルファチャンネルは保持
    output.color = float4(adjustedColor, originalColor.a);
    
    return output;
}
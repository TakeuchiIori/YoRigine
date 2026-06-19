#ifndef POST_EFFECT_CS_HLSLI
#define POST_EFFECT_CS_HLSLI

// UAV書き込み時にsRGBエンコードする (UAVは UNORM 型で SRGB 自動変換が効かないため、
// 手動でガンマカーブを適用してから書き出す)
float3 LinearToSRGB(float3 c)
{
    return pow(saturate(c), 1.0f / 2.2f);
}

// UNORM SRV から sRGB バイトを読んで線形値が必要な場合に使う (基本は SRGB ビューを通せば不要)
float3 SRGBToLinear(float3 c)
{
    return pow(saturate(c), 2.2f);
}

#endif

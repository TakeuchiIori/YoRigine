#ifndef POST_EFFECT_CS_HLSLI
#define POST_EFFECT_CS_HLSLI

// HDR リニアパイプラインでは中間バッファ（R16F）にリニア値をそのまま格納する。
// 各ポスト CS が末尾で呼ぶこの関数はパススルー（恒等）にして、sRGB エンコードと
// トーンマップは最終 blit（CopyImage.PS）に一元化する。
// ※ saturate を残すと HDR(>1) が潰れて Bloom の選択性が失われるため撤去済み。
float3 LinearToSRGB(float3 c)
{
    return c;
}

// UNORM SRV から sRGB バイトを読んで線形値が必要な場合に使う (基本は SRGB ビューを通せば不要)
float3 SRGBToLinear(float3 c)
{
    return pow(saturate(c), 2.2f);
}

#endif

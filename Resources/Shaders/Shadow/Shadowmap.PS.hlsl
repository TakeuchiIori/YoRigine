#include "Shadowmap.hlsli"

struct PSInput
{
    float4 svpos : SV_POSITION;
};

float4 main(PSInput pin) : SV_TARGET
{
    // シャドウパスではカラーは使わない
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
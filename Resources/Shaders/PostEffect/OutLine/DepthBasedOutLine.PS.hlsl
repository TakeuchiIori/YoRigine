#include "../FullScreen/FullScreen.hlsli"
struct Material
{
    float4x4 projectionInverse;
    int kernelSize;
    float4 outlineColor;
};



static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};
static const float kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};





static const float kPrewittHorizontalKernel5x5[5][5] =
{
    { -1.0f / 15.0f, -1.0f / 15.0f, 0.0f, 1.0f / 15.0f, 1.0f / 15.0f },
    { -1.0f / 15.0f, -1.0f / 15.0f, 0.0f, 1.0f / 15.0f, 1.0f / 15.0f },
    { -1.0f / 15.0f, -1.0f / 15.0f, 0.0f, 1.0f / 15.0f, 1.0f / 15.0f },
    { -1.0f / 15.0f, -1.0f / 15.0f, 0.0f, 1.0f / 15.0f, 1.0f / 15.0f },
    { -1.0f / 15.0f, -1.0f / 15.0f, 0.0f, 1.0f / 15.0f, 1.0f / 15.0f }
};
static const float kPrewittVerticalKernel5x5[5][5] =
{
    { -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f },
    { -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f, -1.0f / 15.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f },
    { 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f }
};





static const float kPrewittHorizontalKernel7x7[7][7] =
{
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, 0.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f }
};
static const float kPrewittVerticalKernel7x7[7][7] =
{
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f },
    { -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f, -1.0f / 28.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f },
    { 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f }
};




static const float kPrewittHorizontalKernel9x9[9][9] =
{
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, 0.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f }
};
static const float kPrewittVerticalKernel9x9[9][9] =
{
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f },
    { -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f, -1.0f / 45.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f },
    { 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f, 1.0f / 45.0f }
};







static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
};
static const float kKernel3x3[3][3] =
{
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
};





static const float2 kIndex5x5[5][5] =
{
    { { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f } },
    { { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f } },
    { { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f } },
    { { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f } },
    { { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f } },
};
static const float kKernel5x5[5][5] =
{
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
};





static const float2 kIndex7x7[7][7] =
{
    { { -3.0f, -3.0f }, { -2.0f, -3.0f }, { -1.0f, -3.0f }, { 0.0f, -3.0f }, { 1.0f, -3.0f }, { 2.0f, -3.0f }, { 3.0f, -3.0f } },
    { { -3.0f, -2.0f }, { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f }, { 3.0f, -2.0f } },
    { { -3.0f, -1.0f }, { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f }, { 3.0f, -1.0f } },
    { { -3.0f, 0.0f }, { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f } },
    { { -3.0f, 1.0f }, { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f }, { 3.0f, 1.0f } },
    { { -3.0f, 2.0f }, { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 2.0f } },
    { { -3.0f, 3.0f }, { -2.0f, 3.0f }, { -1.0f, 3.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 3.0f } },
};
static const float kKernel7x7[7][7] =
{
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
};





static const float2 kIndex9x9[9][9] =
{
    { { -4.0f, -4.0f }, { -3.0f, -4.0f }, { -2.0f, -4.0f }, { -1.0f, -4.0f }, { 0.0f, -4.0f }, { 1.0f, -4.0f }, { 2.0f, -4.0f }, { 3.0f, -4.0f }, { 4.0f, -4.0f } },
    { { -4.0f, -3.0f }, { -3.0f, -3.0f }, { -2.0f, -3.0f }, { -1.0f, -3.0f }, { 0.0f, -3.0f }, { 1.0f, -3.0f }, { 2.0f, -3.0f }, { 3.0f, -3.0f }, { 4.0f, -3.0f } },
    { { -4.0f, -2.0f }, { -3.0f, -2.0f }, { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f }, { 3.0f, -2.0f }, { 4.0f, -2.0f } },
    { { -4.0f, -1.0f }, { -3.0f, -1.0f }, { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f }, { 3.0f, -1.0f }, { 4.0f, -1.0f } },
    { { -4.0f, 0.0f }, { -3.0f, 0.0f }, { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f }, { 4.0f, 0.0f } },
    { { -4.0f, 1.0f }, { -3.0f, 1.0f }, { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f }, { 3.0f, 1.0f }, { 4.0f, 1.0f } },
    { { -4.0f, 2.0f }, { -3.0f, 2.0f }, { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 2.0f }, { 4.0f, 2.0f } },
    { { -4.0f, 3.0f }, { -3.0f, 3.0f }, { -2.0f, 3.0f }, { -1.0f, 3.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 3.0f }, { 4.0f, 3.0f } },
    { { -4.0f, 4.0f }, { -3.0f, 4.0f }, { -2.0f, 4.0f }, { -1.0f, 4.0f }, { 0.0f, 4.0f }, { 1.0f, 4.0f }, { 2.0f, 4.0f }, { 3.0f, 4.0f }, { 4.0f, 4.0f } },
};
static const float kKernel9x9[9][9] =
{
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
};





Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);

SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

ConstantBuffer<Material> gMaterial : register(b0);
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(rcp(float(width)), rcp(float(height)));
    float2 difference = float2(0.0f, 0.0f);
    
    if (gMaterial.kernelSize == 3)
    {

        for (int x = 0; x < 3; ++x)
        {
            for (int y = 0; y < 3; ++y)
            {
                float2 texcoord = input.texCoord + kIndex3x3[x][y] * uvStepSize;
                float ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
                float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
                float viewZ = viewSpace.z * rcp(viewSpace.w);
                difference.x += viewZ * kPrewittHorizontalKernel[x][y];
                difference.y += viewZ * kPrewittVerticalKernel[x][y];
            }
        }
    }
    else if (gMaterial.kernelSize == 5)
    {
        for (int x = 0; x < 5; ++x)
        {
            for (int y = 0; y < 5; ++y)
            {
                float2 texcoord = input.texCoord + kIndex5x5[x][y] * uvStepSize;
                float ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
                float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
                float viewZ = viewSpace.z * rcp(viewSpace.w);
                difference.x += viewZ * kPrewittHorizontalKernel5x5[x][y];
                difference.y += viewZ * kPrewittVerticalKernel5x5[x][y];
            }
        }
    }
    else if (gMaterial.kernelSize == 7)
    {
        for (int x = 0; x < 7; ++x)
        {
            for (int y = 0; y < 7; ++y)
            {
                float2 texcoord = input.texCoord + kIndex7x7[x][y] * uvStepSize;
                float ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
                float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
                float viewZ = viewSpace.z * rcp(viewSpace.w);
                difference.x += viewZ * kPrewittHorizontalKernel7x7[x][y];
                difference.y += viewZ * kPrewittVerticalKernel7x7[x][y];
            }
        }
    }
    else if (gMaterial.kernelSize == 9)
    {
        for (int x = 0; x < 9; ++x)
        {
            for (int y = 0; y < 9; ++y)
            {
                float2 texcoord = input.texCoord + kIndex9x9[x][y] * uvStepSize;
                float ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
                float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
                float viewZ = viewSpace.z * rcp(viewSpace.w);
                difference.x += viewZ * kPrewittHorizontalKernel9x9[x][y];
                difference.y += viewZ * kPrewittVerticalKernel9x9[x][y];
            }
        }
    }
    
    
    
    
    
    float weight = length(difference);
    weight = saturate(weight);
    
    PixelShaderOutput output;
    output.color.rgb = lerp(gMaterial.outlineColor.rgb, gTexture.Sample(gSampler, input.texCoord).rgb, 1.0f - weight);
    output.color.a = 1.0f;
    
    return output;
}
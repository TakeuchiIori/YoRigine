#ifndef CASCADE_HLSLI
#define CASCADE_HLSLI

// カスケードシャドウの枚数（C++ DsvManager::kShadowCascadeCount と一致させること）
#define CASCADE_COUNT 3

// カラーパス（PS）用: 全カスケードのライト VP 行列
struct CascadeShadow
{
    float4x4 lightViewProjection[CASCADE_COUNT];
};

// worldPos から最も近い（＝高精細な）カスケードを選び、3x3 PCF で遮蔽率を返す。
//   戻り値: 1.0 = 日向 / 0.0 = 影
//   どのカスケードにも収まらない場合は 1.0（影なし）。
//   端は次のカスケードへ委ねるため margin ぶん内側で判定し、継ぎ目を目立たなくする。
float SampleCascadeShadow(
    Texture2DArray<float> shadowMap,
    SamplerComparisonState shadowSampler,
    CascadeShadow cascade,
    float3 worldPos,
    float shadowMapSize)
{
    const float texel = 1.0f / shadowMapSize;

    [unroll]
    for (int c = 0; c < CASCADE_COUNT; ++c)
    {
        float4 sp = mul(float4(worldPos, 1.0f), cascade.lightViewProjection[c]);
        float3 proj = sp.xyz / sp.w;

        float2 uv;
        uv.x = proj.x * 0.5f + 0.5f;
        uv.y = -proj.y * 0.5f + 0.5f;

        // 最遠カスケードは全域を使う。それ以外は端を次カスケードへ委ねる。
        float margin = (c < CASCADE_COUNT - 1) ? 0.02f : 0.0f;

        if (uv.x >= margin && uv.x <= 1.0f - margin &&
            uv.y >= margin && uv.y <= 1.0f - margin &&
            proj.z <= 1.0f)
        {
            float depth = proj.z - 0.0005f;
            float sum = 0.0f;
            [unroll]
            for (int sy = -1; sy <= 1; ++sy)
            {
                [unroll]
                for (int sx = -1; sx <= 1; ++sx)
                {
                    float2 off = float2(sx, sy) * texel;
                    sum += shadowMap.SampleCmpLevelZero(shadowSampler, float3(uv + off, c), depth);
                }
            }
            return sum / 9.0f;
        }
    }
    return 1.0f;
}

#endif

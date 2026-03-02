#include "../FullScreen/FullScreen.hlsli"
Texture2D sceneTex : register(t0);
Texture2D crackTex : register(t1);
SamplerState smp : register(s0);

cbuffer cbPostEffect : register(b0)
{
    float progress; // 0.0 → 1.0
    float2 resolution;
    float time;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 疑似乱数生成
float random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float2 uv = input.texCoord;
    
    // ヒビ割れテクスチャから破片を定義
    float crack = crackTex.Sample(smp, uv).r;
    float fragmentID = floor(crack * 40.0) / 40.0;
    
    // 破片ごとの乱数
    float2 seed = float2(fragmentID * 12.34, fragmentID * 56.78);
    float randX = random(seed) * 2.0 - 1.0;
    float randY = random(seed + 0.5) * 2.0 - 1.0;
    float randRot = random(seed + 1.0) * 6.28;
    float randSpeed = 0.7 + random(seed + 2.0) * 0.6;
    
    // 進行度を3段階に分割
    // 0.0-0.4: 割れて飛び散る（旧シーン）
    // 0.4-0.6: 暗転（シーン切り替え）
    // 0.6-1.0: 破片が消える（新シーン表示）
    
    float t = 0.0;
    float darkness = 0.0;
    
    if (progress < 0.4)
    {
        // Phase 1: 割れて飛び散る
        t = progress / 0.4;
        t = t * t; // イージング
        darkness = t * 0.5; // 少しずつ暗く
    }
    else if (progress < 0.6)
    {
        // Phase 2: 暗転（破片は最大に飛び散った状態を維持）
        t = 1.0;
        float darkProgress = (progress - 0.4) / 0.2;
        darkness = 0.5 + darkProgress * 0.5; // 完全に暗く
    }
    else
    {
        // Phase 3: 破片が消えて新シーン表示
        float fadeProgress = (progress - 0.6) / 0.4;
        t = 1.0 - fadeProgress; // 破片が徐々に消える
        darkness = 1.0 - fadeProgress; // 徐々に明るく
    }
    
    // 中心からの放射方向に飛び散る
    float2 center = float2(0.5, 0.5);
    float2 toEdge = uv - center;
    float2 direction = normalize(toEdge + float2(randX, randY) * 0.3);
    
    // 移動距離
    float moveAmount = t * randSpeed * 0.5;
    float2 displacement = direction * moveAmount;
    
    // 重力
    displacement.y += t * t * 0.3;
    
    // 回転
    float2 uvCentered = uv - center;
    float angle = randRot * t;
    float c = cos(angle);
    float s = sin(angle);
    float2 rotatedUV = float2(
        uvCentered.x * c - uvCentered.y * s,
        uvCentered.x * s + uvCentered.y * c
    ) + center;
    
    // 最終UV
    float2 finalUV = rotatedUV + displacement;
    
    // シーンをサンプル
    float4 color = sceneTex.Sample(smp, finalUV);
    
    // 画面外チェック
    float inBounds = (finalUV.x >= 0.0 && finalUV.x <= 1.0 &&
                      finalUV.y >= 0.0 && finalUV.y <= 1.0) ? 1.0 : 0.0;
    
    // 割れ目の検出
    float2 pixelSize = 1.0 / resolution;
    float diff = abs(crack - crackTex.Sample(smp, uv + float2(pixelSize.x, 0)).r)
               + abs(crack - crackTex.Sample(smp, uv + float2(0, pixelSize.y)).r);
    
    float isEdge = smoothstep(0.03, 0.06, diff);
    
    // 割れ目を黒くする（Phase 1と2のみ）
    if (progress < 0.6)
    {
        color.rgb = lerp(color.rgb, float3(0, 0, 0), isEdge * min(t, 1.0));
    }
    
    // 境界外は黒
    color.rgb *= inBounds;
    
    // 全体の暗転処理
    color.rgb *= (1.0 - darkness);
    
    output.color = color;
    return output;
}
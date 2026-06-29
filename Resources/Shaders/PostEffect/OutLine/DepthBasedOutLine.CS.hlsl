#include "../Common/PostEffectCS.hlsli"

// 深度 / 法線 / 輝度 の3種のエッジを個別トグルで合成する輪郭線。
// 法線は G-buffer(ワールド空間法線) を参照する。合成は加重合計。
struct Material
{
    float4x4 projectionInverse; // 深度→ビューZ 復元用
    int   kernelSize;           // 深度 Prewitt のカーネルサイズ(奇数)
    int   useDepth;             // 各エッジの有効フラグ
    int   useNormal;
    int   useLuminance;
    float depthWeight;          // 各エッジの重み
    float normalWeight;
    float luminanceWeight;
    float edgeStrength;         // 全体の強度倍率
    float depthThreshold;       // 各エッジの正規化しきい値
    float normalThreshold;
    float luminanceThreshold;
    float normalWidth;          // 法線エッジの太さ(近傍サンプルのピクセル距離。1=隣接)
    float4 outlineColor;        // 輪郭線の色
    float luminanceWidth;       // 輝度エッジの太さ(同上)
    float distanceFadeStart;    // 距離フェード開始(ビュー空間距離)。ここから線が薄くなり始める
    float distanceFadeEnd;      // 距離フェード終了。これより遠いと線を消す(真っ黒化防止)
    int   useDistanceFade;      // 距離フェード有効
};

Texture2D<float4> gInput         : register(t0); // シーンカラー
Texture2D<float>  gDepthTexture  : register(t1); // 深度
Texture2D<float4> gNormalTexture : register(t2); // 法線 G-buffer(.xyz=法線, .w=マスク)
SamplerState gSampler      : register(s0);
SamplerState gSamplerPoint : register(s1);
RWTexture2D<float4> gOutput : register(u0);
ConstantBuffer<Material> gMaterial : register(b0);

float SampleViewZ(float2 uv)
{
    float ndcDepth = gDepthTexture.SampleLevel(gSamplerPoint, uv, 0);
    float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
    return viewSpace.z * rcp(viewSpace.w);
}

float Luminance(float3 c)
{
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;

    float2 uvStep = float2(rcp(float(w)), rcp(float(h)));
    float2 uv = (float2(dtid.xy) + 0.5f) / float2(w, h);

    float depthEdge = 0.0f;
    float normalEdge = 0.0f;
    float lumEdge = 0.0f;

    // === 深度エッジ（Prewitt, 任意 kernelSize） ===
    if (gMaterial.useDepth != 0)
    {
        int r = gMaterial.kernelSize / 2;
        int n = 2 * r + 1;
        float invH = rcp(float(n * n - n));
        float2 diff = float2(0, 0);
        [loop]
        for (int y = -r; y <= r; ++y)
        {
            [loop]
            for (int x = -r; x <= r; ++x)
            {
                float viewZ = SampleViewZ(uv + float2(x, y) * uvStep);
                float hSign = (x > 0) ? 1.0f : (x < 0 ? -1.0f : 0.0f);
                float vSign = (y > 0) ? 1.0f : (y < 0 ? -1.0f : 0.0f);
                diff.x += viewZ * hSign * invH;
                diff.y += viewZ * vSign * invH;
            }
        }
        float g = length(diff);
        depthEdge = saturate(g / max(gMaterial.depthThreshold, 1e-5f)) * gMaterial.depthWeight;
    }

    // === 法線エッジ（4近傍との 1 - dot を加算。両方が有効法線の時のみ） ===
    if (gMaterial.useNormal != 0)
    {
        float4 nC = gNormalTexture.SampleLevel(gSamplerPoint, uv, 0);
        if (nC.w > 0.0f)
        {
            float3 c = normalize(nC.xyz);
            float acc = 0.0f;
            // 太さ：近傍サンプルをこのピクセル距離で取る(>1 で線が太くなる)
            float nWidth = max(gMaterial.normalWidth, 1.0f);
            const float2 offs[4] = {
                float2(-1, 0), float2(1, 0), float2(0, -1), float2(0, 1)
            };
            [unroll]
            for (int i = 0; i < 4; ++i)
            {
                float4 nn = gNormalTexture.SampleLevel(gSamplerPoint, uv + offs[i] * uvStep * nWidth, 0);
                if (nn.w > 0.0f)
                {
                    acc += 1.0f - saturate(dot(c, normalize(nn.xyz)));
                }
            }
            normalEdge = saturate(acc / max(gMaterial.normalThreshold, 1e-5f)) * gMaterial.normalWeight;
        }
    }

    // === 輝度エッジ（4近傍の輝度差） ===
    if (gMaterial.useLuminance != 0)
    {
        // 太さ：近傍サンプルをこのピクセル距離で取る(>1 で線が太くなる)
        float lWidth = max(gMaterial.luminanceWidth, 1.0f);
        float lC = Luminance(gInput.SampleLevel(gSampler, uv, 0).rgb);
        float lL = Luminance(gInput.SampleLevel(gSampler, uv + float2(-1, 0) * uvStep * lWidth, 0).rgb);
        float lR = Luminance(gInput.SampleLevel(gSampler, uv + float2(1, 0) * uvStep * lWidth, 0).rgb);
        float lU = Luminance(gInput.SampleLevel(gSampler, uv + float2(0, -1) * uvStep * lWidth, 0).rgb);
        float lD = Luminance(gInput.SampleLevel(gSampler, uv + float2(0, 1) * uvStep * lWidth, 0).rgb);
        float g = abs(lR - lL) + abs(lD - lU);
        lumEdge = saturate(g / max(gMaterial.luminanceThreshold, 1e-5f)) * gMaterial.luminanceWeight;
    }

    // === 加重合計 ===
    float weight = saturate((depthEdge + normalEdge + lumEdge) * gMaterial.edgeStrength);

    // === 距離フェード ===
    // スクリーン空間の線は固定ピクセル幅なので、遠ざかると小さく映ったオブジェクトを
    // 線が覆い尽くして真っ黒に見える。ビュー空間距離で線を徐々に消して防ぐ。
    if (gMaterial.useDistanceFade != 0 && gMaterial.distanceFadeEnd > gMaterial.distanceFadeStart)
    {
        float dist = abs(SampleViewZ(uv));
        float fade = saturate(1.0f - (dist - gMaterial.distanceFadeStart)
                                   / (gMaterial.distanceFadeEnd - gMaterial.distanceFadeStart));
        weight *= fade;
    }

    float3 sceneColor = gInput.SampleLevel(gSampler, uv, 0).rgb;
    float3 result = lerp(sceneColor, gMaterial.outlineColor.rgb, weight);

    result = LinearToSRGB(result);
    gOutput[dtid.xy] = float4(result, 1.0f);
}

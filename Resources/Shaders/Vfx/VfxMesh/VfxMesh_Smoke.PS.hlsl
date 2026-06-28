#include "VfxMesh_Common.hlsli"

// Valorant Omen 風ボリュームスモーク（レイマーチ版）。
// 球を境界ボリュームとして、ピクセル毎にカメラからレイを飛ばし、
// 球の内部を一定ステップで進みながら 3D ノイズ密度を積分する。
// → 表面テクスチャ感が消え、視点移動で内部が視差で動く本物の体積感になる。
//
// ・NoCull のまま、シェーダ内で「奥側(far)フェイスのみ」処理して二重積分を防ぐ。
//   これによりカメラが球の中に入っても破綻しない。
// ・color.rgb を HDR にすれば Bloom が乗る。リム(縁)は太陽フレア風に発光。

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<SmokeParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 3 平面合成の擬似 3D ノイズ
float Noise3(float3 p)
{
    float xy = ValueNoise(p.xy);
    float yz = ValueNoise(p.yz + 19.7f);
    float zx = ValueNoise(p.zx - 7.3f);
    return (xy + yz + zx) * (1.0f / 3.0f);
}

float FBM3(float3 p, int octaves)
{
    float val = 0.0f, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        val  += amp * Noise3(p * freq);
        amp  *= 0.5f;
        freq *= 2.0f;
    }
    return val;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 center = gMeshParam.center;
    float  radius = max(gMeshParam.radius, 0.001f);

    float3 ro = gCamera.worldPosition;                 // レイ原点 = カメラ
    float3 rd = normalize(input.worldPosition - ro);   // レイ方向

    // 奥側フェイスのみ処理（手前フェイスは捨てて二重積分を防ぐ / 内側でも成立）
    float3 surfN = normalize(input.worldPosition - center);
    if (dot(surfN, normalize(ro - input.worldPosition)) > 0.0f)
    {
        discard;
    }

    // レイ×球の交差
    float3 oc = ro - center;
    float  b  = dot(oc, rd);
    float  c  = dot(oc, oc) - radius * radius;
    float  disc = b * b - c;
    if (disc < 0.0f) { discard; }
    float sq = sqrt(disc);
    float tStart = max(-b - sq, 0.0f);  // 球入口 or カメラ(内側)
    float tEnd   = -b + sq;             // 球出口(奥側)
    if (tEnd <= tStart) { discard; }

    int   oct = (int)clamp(gMeshParam.noiseOctaves, 1.0f, 5.0f);
    float tt  = gMeshParam.time * gMeshParam.scrollSpeed;

    const int STEPS = 24;
    float dt = (tEnd - tStart) / STEPS;
    float t  = tStart + dt * 0.5f;

    float  alpha = 0.0f;
    float3 col   = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int i = 0; i < STEPS; ++i)
    {
        float3 p     = ro + rd * t;
        float3 local = (p - center) / radius;      // -1..1
        float  rr    = length(local);
        // 境界に向かって柔らかく減衰（fresnelPower を“縁の柔らかさ”として流用）
        float  shell = pow(saturate(1.0f - rr), max(gMeshParam.fresnelPower, 0.001f));

        // ドメインワープ（座標をノイズでずらして渦巻かせる）＋時間ドリフト
        float3 q = local * gMeshParam.noiseScale;
        float3 warp = float3(
            Noise3(q + float3(tt, tt * 0.6f, -tt * 0.4f)),
            Noise3(q.yzx - float3(tt * 0.8f, tt * 0.3f, tt * 0.5f)),
            Noise3(q.zxy + float3(tt * 0.2f, -tt, tt * 0.7f))
        );
        float n = FBM3(q + (warp - 0.5f) * 2.5f + tt, oct);
        n = saturate(n);

        float dens = lerp(1.0f, n, saturate(gMeshParam.noiseStrength)) * shell;

        // 1 ステップ分の不透明度（front-to-back 合成）
        float a = saturate(dens * gMeshParam.density * dt * 1.6f);
        float3 stepCol = gMeshParam.color.rgb * (0.6f + 0.8f * n);

        col   += (1.0f - alpha) * a * stepCol;
        alpha += (1.0f - alpha) * a;

        if (alpha > 0.99f) break;
        t += dt;
    }

    // 太陽フレア風リム：レイが球の縁をかすめるほど明るく → Bloom で外へ滲む
    float closest = length(oc - rd * b);             // レイと中心の最接近距離
    float graze   = saturate(closest / radius);      // 0=中心貫通, 1=縁
    float rim     = pow(graze, 4.0f) * gMeshParam.rimIntensity;
    col   += gMeshParam.color.rgb * rim;
    alpha  = saturate(alpha + rim * 0.35f);

    if (alpha < 0.004f) { discard; }

    // front-to-back の col はプリマルチ済み → SrcAlpha ブレンド用に非プリマルチ化
    output.color = float4(col / max(alpha, 1e-4f), alpha * gMeshParam.color.a);
    return output;
}

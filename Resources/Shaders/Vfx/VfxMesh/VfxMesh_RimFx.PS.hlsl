#include "VfxMesh_Common.hlsli"

// 円の縁(RimCurtain)から上へ立ち上がる縦演出。
//   texcoord.x = 円周まわり(0..1) / texcoord.y(=v) = 下(0)→上(1)
//   age        = 縁の縦横比 aspect(=円周/高さ)。潰れ防止に等方座標へ変換して使う。
//   style で 8 種類の表現を切替（0=炎 / 1=霊気 / 2=電撃 / 3=花びら /
//                                4=オーラ柱 / 5=毒 / 6=渦 / 7=火の粉）。
// 色は color(根本)→tipColor(先端) を高さ/強度で補間。加算ブレンド・HDR で Bloom。

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<RimFxParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ============================================================
// style 0: 炎（下から燃え上がり、舌状に途切れる）
// ============================================================
float S_Flame(float u, float v, float t)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;
    const float turb = gMeshParam.turbulence;

    float2 nuv = float2(u * ns, v * ns - t * rise);
    float n  = FBM(nuv, 4);
    float n2 = FBM(nuv * 2.0f + float2(0.0f, -t * rise * 1.7f), 2);
    float noise = lerp(n, n2, 0.35f) * turb;

    float body    = saturate((1.0f - v) * 1.6f);
    float tongues = saturate(noise * 1.7f - v * 1.3f);
    float flame   = saturate(body * 0.45f + tongues);

    flame *= smoothstep(1.0f, 0.65f, v);
    return flame;
}

// ============================================================
// style 1: 霊気（背が高くゆらめく柔らかな魂の柱）
// ============================================================
float S_Wisp(float u, float v, float t)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    float sway = sin(v * 4.0f + t * 1.2f + u * 20.0f) * 0.06f * gMeshParam.turbulence;
    float2 nuv = float2((u + sway) * ns * 0.5f, v * ns * 0.7f - t * rise * 0.5f);
    float n = FBM(nuv, 3);

    float col = saturate(n * 1.35f - 0.25f);
    col *= smoothstep(0.0f, 0.20f, v);
    col *= smoothstep(1.05f, 0.30f, v);
    return col * 0.75f;
}

// ============================================================
// style 2: 電撃（細く明滅する縦の稲妻。本数は noiseScale で調整）
// ============================================================
float S_Electric(float u, float v, float t)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    // 本数 = noiseScale。倍率を上げて最大 ~48 本まで出せるように。
    float lineFreq = ns * 3.0f;

    float jitter = FBM(float2(v * 3.0f - t * rise * 2.0f, 0.0f), 2) * 0.15f * (0.5f + gMeshParam.turbulence);
    float d    = abs(frac(u * lineFreq + jitter) - 0.5f);
    float bolt = saturate(1.0f - d * 24.0f);

    float flick = 0.4f + 0.6f * abs(sin(t * 25.0f + floor(u * lineFreq) * 11.0f));

    float body = bolt * flick;
    body *= smoothstep(1.0f, 0.75f, v) * smoothstep(0.0f, 0.04f, v);
    return saturate(body * 1.3f);
}

// ============================================================
// style 3: 花びら（上へ舞い上がる柔らかな粒/花片）
//   aspect(asp)で等方座標にして、縁が低くても潰れないようにする。
// ============================================================
float S_Petal(float u, float v, float t, float asp)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    float2 cell = float2(u * asp, v) * ns * 1.2f; // 高さ基準の等方セル
    cell.y -= t * rise;
    float2 id = floor(cell);
    float2 f  = frac(cell) - 0.5f;
    float2 r  = Hash22(id) - 0.5f;

    float d    = length(f - r * 0.4f);
    float blob = saturate(1.0f - d * 3.0f);
    blob *= smoothstep(1.0f, 0.40f, v) * smoothstep(0.0f, 0.10f, v);
    return blob * 0.9f;
}

// ============================================================
// style 4: オーラ柱（滑らかで均一な発光柱・脈動）
// ============================================================
float S_Aura(float u, float v, float t)
{
    const float rise = gMeshParam.riseSpeed;

    float grad  = smoothstep(1.0f, 0.0f, v);
    float pulse = 0.80f + 0.20f * sin(t * 2.0f * rise + u * 6.2831f * 2.0f);
    float wob   = 0.90f + 0.10f * FBM(float2(u * 4.0f, v * 2.0f - t * rise), 2) * gMeshParam.turbulence;
    return grad * pulse * wob;
}

// ============================================================
// style 5: 毒（ゆっくり湧き上がる不気味な泡）
// ============================================================
float S_Poison(float u, float v, float t, float asp)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    float2 nuv = float2(u * asp * 0.25f, v) * ns; // 横も高さ基準で程よく
    nuv.y -= t * rise * 0.5f;
    float n  = FBM(nuv, 4);
    float n2 = FBM(nuv * 2.3f + 5.0f, 2);

    float bub = saturate(pow(saturate(n * 1.4f - v * 0.5f), 1.5f) + n2 * 0.2f * gMeshParam.turbulence);
    bub *= smoothstep(1.0f, 0.50f, v) * smoothstep(0.0f, 0.08f, v);
    return bub;
}

// ============================================================
// style 6: 渦（螺旋を描いて回りながら立ち上るエネルギー束）
//   束の本数は noiseScale。時間で回転＋上昇して常に動く。
// ============================================================
float S_Vortex(float u, float v, float t)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    float strands = max(floor(ns), 2.0f);
    // u を高さでねじって螺旋に、時間で全体を回す
    float sp = frac((u + v * 0.6f + t * rise * 0.15f) * strands);
    float d  = abs(sp - 0.5f);
    float strand = saturate(1.0f - d * 8.0f);

    // 螺旋に沿って上へ流れる輝きの脈動
    strand *= 0.55f + 0.45f * sin((v * ns - t * rise * 2.0f) * 6.2831f);
    strand *= smoothstep(0.0f, 0.08f, v) * smoothstep(1.0f, 0.45f, v);
    return saturate(strand * (0.8f + 0.4f * gMeshParam.turbulence));
}

// ============================================================
// style 7: 火の粉（まばらに舞い上がる火花/残り火）
// ============================================================
float S_Sparkle(float u, float v, float t, float asp)
{
    const float ns = gMeshParam.noiseScale;
    const float rise = gMeshParam.riseSpeed;

    float2 uv = float2(u * asp, v) * ns * 2.0f; // 高さ基準で丸い火花に
    uv.y -= t * rise * 0.6f;                     // 上へ流れる
    float2 cell = floor(uv);
    float2 r    = Hash22(cell);

    float twinkle = pow(abs(sin(t * 6.0f * rise + r.x * 6.2831f)), 8.0f);
    float present = step(0.55f, r.y);
    float2 f      = frac(uv) - 0.5f;
    float pt      = saturate(1.0f - length(f) * 3.0f);

    float e = pt * twinkle * present;
    e *= smoothstep(1.0f, 0.30f, v);
    return e * 1.5f;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float u = input.texcoord.x;
    float v = saturate(input.texcoord.y);
    float t = gMeshParam.time;
    float asp = max(input.age, 1.0f); // 縁の縦横比(円周/高さ)。潰れ防止用。

    int   style = (int)(gMeshParam.style + 0.5f);
    float intensity = 0.0f;

    switch (style)
    {
        case 1:  intensity = S_Wisp(u, v, t);          break;
        case 2:  intensity = S_Electric(u, v, t);      break;
        case 3:  intensity = S_Petal(u, v, t, asp);    break;
        case 4:  intensity = S_Aura(u, v, t);          break;
        case 5:  intensity = S_Poison(u, v, t, asp);   break;
        case 6:  intensity = S_Vortex(u, v, t);        break;
        case 7:  intensity = S_Sparkle(u, v, t, asp);  break;
        default:
        case 0:  intensity = S_Flame(u, v, t);         break;
    }

    // テクスチャ（任意）: 強度に乗せるマスク/模様。texStrength=0 なら完全に無効。
    if (gMeshParam.texStrength > 0.001f)
    {
        float2 tuv = float2(u * gMeshParam.texTiling,
                            v * gMeshParam.texTiling - t * gMeshParam.texScroll);
        float texL = gTexNoise.Sample(gSampler, tuv).r; // 輝度(赤)をマスクに使う
        intensity *= lerp(1.0f, texL, saturate(gMeshParam.texStrength));
    }

    intensity *= gMeshParam.color.a;
    if (intensity < 0.004f) { discard; }

    // 根本(熱い color) → 先端(tipColor) を強度/高さで補間
    float mixT = saturate(v * 1.1f + (1.0f - intensity) * 0.3f);
    float3 col = lerp(gMeshParam.color.rgb, gMeshParam.tipColor.rgb, mixT);
    col *= intensity;

    // 加算プリマルチ
    output.color = float4(col, saturate(intensity));
    return output;
}

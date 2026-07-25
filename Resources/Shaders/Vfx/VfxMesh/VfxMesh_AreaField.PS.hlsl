#include "VfxMesh_Common.hlsli"

// 地面円フィールド（バフエリア/AoE/射程マーカー）。
// DiscGeometry の放射UV（中心0.5 → 縁で length(uv*2-1)=1）を受け、
// style(0..7) で 8 種類の魔法陣デザインを切替。さらに ②エネルギー ③スキャンを
// 係数で上乗せできる。加算プリマルチ・HDRでBloomが乗る。
//   style: 0 古典 / 1 同心円 / 2 ルーン時計 / 3 六芒星 /
//          4 五角星印 / 5 花弁 / 6 レーダー / 7 秘術
// 膨張は Module(scale) が Disc を拡大して表現する（シェーダーは固定UV）。

ConstantBuffer<Camera> gCamera : register(b0);
ConstantBuffer<AreaFieldParams> gMeshParam : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

static const float TAU = 6.2831853f;

// 半径 R・幅 w の細いリング（1=線上）
float RingSDF(float r, float R, float w)
{
    return saturate(1.0f - abs(r - R) / max(w, 1e-4f));
}

// count 本の放射線（角度方向の細線）。thickness=線の太さ(0..0.5)
float RadialLines(float ang, float count, float rot, float thickness)
{
    float a  = (ang + rot) / TAU * count;
    float di = min(frac(a), 1.0f - frac(a)); // 最寄り整数までの距離
    return smoothstep(thickness, 0.0f, di);
}

// 正規 n 角形の境界半径（ang 方向）。外周を線で描くのに使う。
float NgonR(float ang, float n, float R, float rot)
{
    float seg = TAU / n;
    float aa  = fmod(ang + rot + TAU * 10.0f, seg) - seg * 0.5f;
    return R * cos(seg * 0.5f) / max(cos(aa), 1e-3f);
}

// ---- 8 デザイン。戻り値は発光ライン強度(0..) ----

// 0: 古典魔法陣（回転花弁 + 中間リング）
float Rune_Classic(float r, float ang, float rot, float rc)
{
    float petals    = pow(0.5f + 0.5f * cos(ang * rc + rot), 6.0f);
    float midBand   = smoothstep(0.25f, 0.5f, r) * (1.0f - smoothstep(0.6f, 0.85f, r));
    float innerRing = RingSDF(r, 0.55f, 0.02f);
    return petals * midBand + innerRing;
}

// 1: 同心円 + 回転する破線リング
float Rune_Rings(float r, float ang, float rot, float rc)
{
    float rings = RingSDF(r, 0.30f, 0.015f) + RingSDF(r, 0.55f, 0.02f) + RingSDF(r, 0.80f, 0.015f);
    float dash  = RingSDF(r, 0.68f, 0.03f);
    float seg   = step(0.5f, frac((ang + rot) / TAU * rc * 2.0f));
    return rings + dash * seg;
}

// 2: ルーン時計（外内リング + スポーク + 外周目盛）
float Rune_Clock(float r, float ang, float rot, float rc)
{
    float rings = RingSDF(r, 0.40f, 0.015f) + RingSDF(r, 0.80f, 0.02f);
    float spoke = RadialLines(ang, rc, rot, 0.02f)
                * smoothstep(0.40f, 0.42f, r) * (1.0f - smoothstep(0.78f, 0.80f, r));
    float ticks = RadialLines(ang, rc * 3.0f, 0.0f, 0.01f) * RingSDF(r, 0.80f, 0.05f);
    return rings + spoke + ticks;
}

// 3: 六芒星（三角形2枚 + 円）
float Rune_Hexagram(float r, float ang, float rot, float rc)
{
    float R    = 0.72f;
    float tri1 = RingSDF(r, NgonR(ang, 3.0f, R, rot), 0.02f);
    float tri2 = RingSDF(r, NgonR(ang, 3.0f, R, rot + TAU / 6.0f), 0.02f);
    float ring = RingSDF(r, R, 0.015f);
    float clip = 1.0f - smoothstep(0.90f, 1.0f, r);
    return (tri1 + tri2 + ring) * clip;
}

// 4: 五角星印（五角形 + 円 + 花弁）
float Rune_Pentagon(float r, float ang, float rot, float rc)
{
    float pent   = RingSDF(r, NgonR(ang, 5.0f, 0.70f, rot), 0.02f);
    float ring   = RingSDF(r, 0.78f, 0.015f);
    float petals = pow(0.5f + 0.5f * cos(ang * 5.0f + rot), 8.0f)
                 * smoothstep(0.20f, 0.40f, r) * (1.0f - smoothstep(0.50f, 0.65f, r));
    float clip   = 1.0f - smoothstep(0.85f, 0.95f, r);
    return (pent + ring + petals) * clip;
}

// 5: 花弁（密な花 + 外周リング）
float Rune_Flower(float r, float ang, float rot, float rc)
{
    float p1   = pow(0.5f + 0.5f * cos(ang * rc + rot), 4.0f);
    float p2   = pow(0.5f + 0.5f * cos(ang * rc * 2.0f - rot * 0.5f), 8.0f);
    float body = smoothstep(0.10f, 0.35f, r) * (1.0f - smoothstep(0.55f, 0.80f, r));
    float ring = RingSDF(r, 0.82f, 0.015f);
    return (p1 * 0.6f + p2 * 0.4f) * body + ring;
}

// 6: レーダー（回転スイープ + レンジリング + 目盛）
float Rune_Radar(float r, float ang, float rot, float rc)
{
    float rings = RingSDF(r, 0.35f, 0.012f) + RingSDF(r, 0.60f, 0.012f) + RingSDF(r, 0.85f, 0.015f);
    float sweepAng = frac((ang - rot) / TAU);          // 0..1 で一周
    float sweep    = smoothstep(0.0f, 0.06f, sweepAng) * (1.0f - sweepAng); // 後ろへ尾を引く
    sweep *= smoothstep(0.0f, 0.05f, r);
    float ticks = RadialLines(ang, rc * 2.0f, 0.0f, 0.008f) * RingSDF(r, 0.85f, 0.05f);
    return rings + sweep * 0.8f + ticks;
}

// 7: 秘術（FBM フィリグリー + 花弁 + 外周リング）
float Rune_Arcane(float r, float ang, float rot, float rc, float t, float ns)
{
    float2 pc  = float2(cos(ang), sin(ang)) * r;
    float  fil = FBM(pc * ns * 1.5f + float2(rot * 0.1f, t * 0.1f), 4);
    float  fw  = smoothstep(0.45f, 0.60f, fil) * smoothstep(0.15f, 0.85f, r);
    float petals = pow(0.5f + 0.5f * cos(ang * rc + rot), 6.0f)
                 * smoothstep(0.30f, 0.50f, r) * (1.0f - smoothstep(0.60f, 0.80f, r));
    float ring = RingSDF(r, 0.85f, 0.015f);
    return fw * 0.7f + petals + ring;
}

// ============================================================
// 内部の質感（fillStyle）。戻り値=内部の明るさ場(0..)。fillOpacity と softEdge を後で乗算。
// ============================================================

// 0: 無地（中心ほど明るい放射グラデ）
float Fill_Plain(float r)
{
    return 0.35f + 0.65f * (1.0f - r);
}

// 1: 毒（ドメインワープ FBM でどくどく泡立つ）
float Fill_Poison(float r, float2 c, float t, float ns)
{
    float2 w = float2(FBM(c * ns + t * 0.30f, 3),
                      FBM(c * ns + 5.2f - t * 0.25f, 3));
    float n   = FBM(c * ns * 1.3f + w * 1.5f + float2(0.0f, -t * 0.40f), 4);
    float bub = smoothstep(0.35f, 0.75f, n);           // 泡の塊
    float base = 0.25f + 0.50f * (1.0f - r);
    return base * (0.45f + 1.0f * bub);                // ボコボコ脈動
}

// 2: バフ（中心の脈動スウェル + 外へ立ち上る光の粒 = 能力上昇感）
float Fill_Buff(float r, float ang, float t, float ns)
{
    float swell = (0.40f + 0.60f * (1.0f - r)) * (0.82f + 0.18f * sin(t * 2.2f));
    float rays  = pow(0.5f + 0.5f * sin(ang * ns * 2.0f), 4.0f);
    float flow  = frac(r * 2.0f - t * 1.2f);
    float motes = smoothstep(0.70f, 1.0f, flow) * (1.0f - r);   // 外へ流れる粒
    return swell + rays * motes * 0.9f;
}

// 3: エネルギー（流れる FBM）
float Fill_Energy(float r, float2 c, float t, float ns)
{
    float e = FBM(c * ns + float2(t * 0.25f, -t * 0.18f), 4);
    return (0.30f + 0.60f * (1.0f - r)) * (0.40f + 1.0f * saturate(e * 1.4f - 0.15f));
}

// 4: 波紋（中心から広がる同心の波）
float Fill_Ripple(float r, float t, float ns)
{
    float w = 0.5f + 0.5f * sin(r * ns * 3.0f - t * 2.0f);
    return (0.30f + 0.60f * (1.0f - r)) * (0.55f + 0.65f * w);
}

// 5: 残り火（ちらつく熱ノイズ。暖色は color で）
float Fill_Ember(float r, float2 c, float t, float ns)
{
    float n     = FBM(c * ns * 1.5f - float2(0.0f, t * 0.60f), 4);
    float flick = 0.6f + 0.4f * sin(t * 8.0f + n * 10.0f);
    return (0.25f + 0.50f * (1.0f - r)) * saturate(n * 1.5f) * flick;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 円中心からの正規化半径（中心0, 縁で1）と角度
    float2 c = input.texcoord * 2.0f - 1.0f;
    float  r = length(c);
    if (r > 1.0f) { discard; }
    float ang = atan2(c.y, c.x);
    float t   = gMeshParam.time;

    // 円の外を柔らかく切る
    float ew       = max(gMeshParam.edgeWidth, 0.002f);
    float softEdge = smoothstep(1.0f, 1.0f - ew * 1.5f, r);

    // ── 外周リム（帯） ─────────────────────────────
    float rim = 1.0f - saturate(abs(r - (1.0f - ew)) / ew);
    rim = pow(rim, 1.5f);

    // ── 内部の質感（fillStyle で切替） ─────────────
    int   fillStyle = (int)(gMeshParam.fillStyle + 0.5f);
    float fillField = 0.0f;
    switch (fillStyle)
    {
        case 1:  fillField = Fill_Poison(r, c, t, gMeshParam.noiseScale);        break;
        case 2:  fillField = Fill_Buff(r, ang, t, gMeshParam.noiseScale);        break;
        case 3:  fillField = Fill_Energy(r, c, t, gMeshParam.noiseScale);        break;
        case 4:  fillField = Fill_Ripple(r, t, gMeshParam.noiseScale);           break;
        case 5:  fillField = Fill_Ember(r, c, t, gMeshParam.noiseScale);         break;
        default:
        case 0:  fillField = Fill_Plain(r);                                      break;
    }
    float baseFill = gMeshParam.fillOpacity * fillField * softEdge;

    // ── 魔法陣デザイン（style で切替） ─────────────
    float rot = t * gMeshParam.ringSpeed * TAU;
    float rc  = gMeshParam.runeCount;
    int   style = (int)(gMeshParam.style + 0.5f);
    float pattern = 0.0f;
    switch (style)
    {
        case 1:  pattern = Rune_Rings(r, ang, rot, rc);                       break;
        case 2:  pattern = Rune_Clock(r, ang, rot, rc);                       break;
        case 3:  pattern = Rune_Hexagram(r, ang, rot, rc);                    break;
        case 4:  pattern = Rune_Pentagon(r, ang, rot, rc);                    break;
        case 5:  pattern = Rune_Flower(r, ang, rot, rc);                      break;
        case 6:  pattern = Rune_Radar(r, ang, rot, rc);                       break;
        case 7:  pattern = Rune_Arcane(r, ang, rot, rc, t, gMeshParam.noiseScale); break;
        default:
        case 0:  pattern = Rune_Classic(r, ang, rot, rc);                     break;
    }
    pattern = saturate(pattern) * softEdge;

    // ── ② エネルギー場: 流れる FBM ノイズ ─────────
    float2 nuv    = c * gMeshParam.noiseScale;
    float  energy = FBM(nuv + float2(t * 0.25f, -t * 0.18f), 4);
    energy = saturate(energy * 1.4f - 0.15f) * softEdge;

    // ── ③ スキャン波: 中心→外周へ走る同心円 ───────
    float sc   = frac(r * 3.0f - t * gMeshParam.scanSpeed);
    float scan = smoothstep(0.82f, 1.0f, sc) * softEdge;

    // ── 合成 ─────────────────────────────────────
    float fill = baseFill;
    fill += gMeshParam.styleEnergy * energy * gMeshParam.fillOpacity * 1.2f;

    float emissive = gMeshParam.styleRune * pattern
                   + gMeshParam.styleScan * scan;

    float3 col = gMeshParam.color.rgb * (fill + emissive);
    col += gMeshParam.edgeColor.rgb * rim;

    float a = saturate((fill + emissive + rim));
    a *= gMeshParam.color.a;
    if (a < 0.004f) { discard; }

    // 加算プリマルチ（不透明度を色に畳んで出力）
    output.color = float4(col * gMeshParam.color.a, a);
    return output;
}


//--------------------------------------------------
// VfxMesh_Common.hlsli
// UE Niagara Ribbon Trail にインスパイアされた
// 共通定義・ユーティリティ
//--------------------------------------------------

//--------------------------------------------------
// 頂点入力
//--------------------------------------------------
struct VertexShaderInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;     // rgba, アルファはフェード済みを渡す
    float  age      : TEXCOORD1;  // 0=生成直後, 1=消滅寸前 (正規化)
};

//--------------------------------------------------
// VS → PS
//--------------------------------------------------
struct VertexShaderOutput
{
    float4 position      : SV_POSITION;
    float2 texcoord      : TEXCOORD0;
    float4 color         : COLOR0;
    float  age           : TEXCOORD1;
    float3 worldPosition : POSITION0;
};

//--------------------------------------------------
// Camera
//--------------------------------------------------
struct Camera
{
    float3   worldPosition;
    float4x4 viewProjection;
};

//--------------------------------------------------
// Trail パラメータ
// ★ UE Niagara 準拠の豪華版
//
// HLSL CB ルール: float4 境界に注意してレイアウト
//   Row0: colorInner
//   Row1: colorOuter
//   Row2: rimColor          (エッジリムカラー / HDR値OK)
//   Row3: softness, glowPower, distortion, time
//   Row4: energyIntensity, energySpeed, sparkleAmount, sparkleSpeed
//   Row5: fresnelStrength, trailSharpness, colorWaveFreq, colorWaveAmp
//   Row6: uvScrollSpeed, noiseOctaves(=float), dissolveStrength, dissolveEdgeWidth
//   Row7: dissolveEdgeColor
//--------------------------------------------------
struct MeshTrailParams
{
    // --- カラー ---
    float4 colorInner;       // 先端側カラー (新しい端, x=0)
    float4 colorOuter;       // 根本側カラー (古い端, x=1)
    float4 rimColor;         // ★NEW: エッジリムカラー (bloom用に >1.0 可)

    // --- 基本制御 ---
    float softness;          // エッジフェード幅
    float glowPower;         // 中心コアグロー強度
    float distortion;        // ノイズ UV 歪み強度
    float time;              // 累積時間

    // ★NEW: エネルギーライン (UE Niagara の電気/魔力ライン)
    float energyIntensity;   // エネルギーライン輝度
    float energySpeed;       // エネルギーラインスクロール速度
    float sparkleAmount;     // スパークル輝度
    float sparkleSpeed;      // スパークルアニメ速度

    // ★NEW: フレネル & 波形
    float fresnelStrength;   // エッジフレネルグロー強度
    float trailSharpness;    // エッジシャープネス (高いほどくっきり)
    float colorWaveFreq;     // カラーウェーブ周波数
    float colorWaveAmp;      // カラーウェーブ振幅 (0=OFF)

    // ★NEW: UV
    float uvScrollSpeed;     // UV スクロール速度
    float noiseOctaves;      // ノイズオクターブ数 (1-4)
    float dissolveStrength;  // ★NEW: 溶けて消える強度 (0=OFF)
    float dissolveEdgeWidth; // ★NEW: 侵食エッジの帯幅

    float4 dissolveEdgeColor;// ★NEW: 侵食エッジのHDR発光色 (Bloom用に >1)

    float emissiveIntensity; // ★NEW: 発光マスター強度 (0=消灯)
    float _pad2;
    float _pad3;
    float _pad4;
};

//--------------------------------------------------
// LightVolume パラメータ (変更なし)
//--------------------------------------------------
struct LightVolumeParams
{
    float4 color;
    float edgeFade;
    float depthFade;
    float noiseTiling;
    float noiseStrength;
    float time;
    float beamStrength;
    float beamRadius;
    float beamPower;
    float beamGlow;
};

//--------------------------------------------------
// Smoke パラメータ (VolumeSmokeMesh — Omen 風ボリュームスモーク)
//   C++ 側 SmokeParamsCB と一致させること
//--------------------------------------------------
struct SmokeParams
{
    float4 color;          // rgb=色(>1でBloom), a=密度基準
    float3 center;         // 球中心(ワールド)
    float  radius;         // 球半径
    float  time;           // アニメ時間
    float  noiseScale;     // FBM タイリング
    float  noiseStrength;  // 渦巻きの強さ 0..1
    float  scrollSpeed;    // ノイズスクロール速度
    float  fresnelPower;   // 縁の柔らかさ
    float  density;        // 全体不透明度倍率
    float  noiseOctaves;   // FBM オクターブ(1-4)
    float  rimIntensity;   // リム発光強度（太陽フレア風 / Bloom 用）
    float  burst;          // -1=継続, 0..1=爆発ワンショット進捗
    float3 _pad2;
    float4 smokeColor;     // 爆発後に遷移する煙色
};

//--------------------------------------------------
// Lightning パラメータ (LightningMesh — プロシージャル稲妻)
//--------------------------------------------------
struct LightningParams
{
    float4 color;            // 芯(コア)の色(HDR)
    float4 glowColor;        // 外側グローの色(HDR) ★2色
    float4 branchColor;      // 枝の色(HDR)
    float  time;             // アニメ時間
    float  glowPower;        // 中心グロー強度(芯の細さ)
    float  coreWidth;        // 芯の太さ/実体感(0..1)
    float  solidness;        // 透明感を減らす(0..1)
    float  outlineIntensity; // 縁/枝のアウトライン強調
    float  _pad0;
    float  _pad1;
    float  _pad2;
};

//--------------------------------------------------
// Shockwave パラメータ (ShockwaveMesh — 爆発の衝撃波リング)
//--------------------------------------------------
struct ShockwaveParams
{
    float4 color;      // rgb>1 で Bloom / a=不透明度(モジュール適用済み)
    float  thickness;  // リング太さ(UV)
    float  ringRadius; // リング位置(UV, 0..1)。膨張は Module(scale) が担当
    float2 _pad;
};

//--------------------------------------------------
// AreaField パラメータ (DiscGeometry × AreaFieldMaterial — 地面円フィールド)
//   C++ 側 AreaFieldParamsCB と一致させること
//--------------------------------------------------
struct AreaFieldParams
{
    float4 color;       // rgb=ベース色(>1でBloom) / a=不透明度
    float4 edgeColor;   // 外周リムの HDR 色
    float  fillOpacity; // 内部塗りの濃さ
    float  edgeWidth;   // 外周リム帯幅(UV)
    float  ringSpeed;   // 魔法陣の回転速度
    float  noiseScale;  // エネルギーノイズのタイリング
    float  scanSpeed;   // スキャン波の速さ
    float  runeCount;   // 魔法陣の対称数
    float  styleRune;   // 魔法陣デザインの強さ（発光ライン）
    float  styleEnergy; // ②エネルギー場ブレンド
    float  styleScan;   // ③スキャン波ブレンド
    float  time;        // アニメ時間
    float  style;       // 魔法陣デザイン種別(0..7)
    float  fillStyle;   // 内部の質感(0..5) 無地/毒/バフ/エネルギー/波紋/残り火
};

//--------------------------------------------------
// RimFx パラメータ (RimCurtain × RimFxMaterial — 縁の縦演出)
//   C++ 側 RimFxParamsCB と一致させること
//--------------------------------------------------
struct RimFxParams
{
    float4 color;       // 根本の色(>1でBloom) / a=強度
    float4 tipColor;    // 先端(上)の色
    float  style;       // 0=炎 / 1=霊気 / 2=電撃 / …
    float  riseSpeed;   // 上昇スクロール速度
    float  noiseScale;  // 揺らぎ/炎舌のタイリング（電撃/渦は本数）
    float  turbulence;  // 揺らぎの強さ
    float  time;        // アニメ時間
    float  texStrength; // テクスチャの寄与(0=無効)
    float  texTiling;   // テクスチャUVタイリング
    float  texScroll;   // テクスチャ縦スクロール速度
};

//--------------------------------------------------
// テクスチャ / サンプラー
//--------------------------------------------------
Texture2D    gTexNoise     : register(t0);
Texture2D    gTexRamp      : register(t1);
SamplerState gSampler      : register(s0); // Linear Wrap
SamplerState gSamplerClamp : register(s1); // Linear Clamp

//==================================================
// ユーティリティ関数
//==================================================

// 0-1 の両端をソフトにフェードさせる
float EdgeFade(float t, float fadeWidth)
{
    float lo = smoothstep(0.0, fadeWidth, t);
    float hi = smoothstep(1.0, 1.0 - fadeWidth, t);
    return lo * hi;
}

// 高品質ハッシュ (UE の Rand2DPCG に相当)
float2 Hash22(float2 p)
{
    p = frac(p * float2(443.897f, 441.423f));
    p += dot(p, p.yx + 19.19f);
    return frac((p.xx + p.yx) * p.xy);
}

float Hash11(float p)
{
    p = frac(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return frac(p);
}

// Value Noise (1 オクターブ)
float ValueNoise(float2 uv)
{
    float2 i = floor(uv);
    float2 f = frac(uv);
    float2 u = f * f * (3.0f - 2.0f * f); // cubic hermine

    float a = Hash22(i            ).x;
    float b = Hash22(i + float2(1,0)).x;
    float c = Hash22(i + float2(0,1)).x;
    float d = Hash22(i + float2(1,1)).x;
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// FBM (フラクショナルブラウン運動) ノイズ
// octaves: 1-4 でディテールを増やす
float FBM(float2 uv, int octaves)
{
    float val  = 0.0f;
    float amp  = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        val  += amp  * ValueNoise(uv * freq);
        amp  *= 0.5f;
        freq *= 2.0f;
    }
    return val;
}

// ★ エネルギーライン (UE Niagara のライトニング風)
// uv.y に沿って走る細い明るいライン
// lineCount: ライン本数, speed: スクロール速度
float EnergyLines(float2 uv, float time, float lineCount, float speed)
{
    float t = uv.y * lineCount - time * speed;

    // 複数ラインの重ね合わせ (周波数違い)
    float line1 = saturate(1.0f - abs(frac(t          ) - 0.5f) * 12.0f);
    float line2 = saturate(1.0f - abs(frac(t * 0.618f + 0.3f) - 0.5f) * 16.0f);
    float line3 = saturate(1.0f - abs(frac(t * 1.414f + 0.7f) - 0.5f) * 20.0f);

    return pow(max(line1, max(line2 * 0.6f, line3 * 0.4f)), 2.0f);
}

// ★ スパークル (UE の TemporalAA ノイズ風)
// ランダムにきらめく点
float Sparkle(float2 uv, float time, float speed, float density)
{
    float2 cell = floor(uv * float2(density * 4.0f, density * 8.0f));
    float2 rnd  = Hash22(cell);
    float  t    = frac(time * speed + rnd.x);
    // sin^16 でシャープなきらめきパルス
    float  pulse = pow(abs(sin(t * 3.14159f)), 16.0f);
    // セル内のランダムな明るさで間引き
    return pulse * step(0.6f, rnd.y);
}

// ★ フレネル近似 (エッジグロー用)
// edgeT: 0=中心, 1=エッジ
float FresnelEdge(float edgeT, float power)
{
    return pow(edgeT, power);
}

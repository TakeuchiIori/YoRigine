#include "GPUParticle.hlsli"

// SoA: hot(t0) と warm(t1) を読み、scale/color は lifeRatio で lerp 導出する
// （Update CS / SpawnTrail CS と同一式）。cold はトレイル生成専用なので VS は読まない。
// インダイレクト描画: instanceID は「生存粒子の通し番号」なので DrawList(t2) で実スロットへ変換する。
// 描画インスタンス数は生存数ぶんだけなので、ここに来る時点で必ず生存＝isActive カリング不要。
StructuredBuffer<ParticleHot>  g_Hot  : register(t0);
StructuredBuffer<ParticleWarm> g_Warm : register(t1);
StructuredBuffer<uint>         g_DrawList : register(t2);
ConstantBuffer<PerView> g_PerView : register(b0);
ConstantBuffer<ParticleExtParams> g_ExtParams : register(b1); // UVScroll/ScalePulse/ColorFlicker（拡張Paramモジュール）

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;

    // ① 生存粒子リストから実スロットを引き、hot を取得
    uint slot = g_DrawList[instanceID];
    ParticleHot hot = g_Hot[slot];

    // ② scale / color を warm から lifeRatio で導出
    ParticleWarm warm = g_Warm[slot];
    float lifeRatio = (hot.lifeTime > 0.0f) ? saturate(hot.currentTime / hot.lifeTime) : 0.0f;
    float3 scale = lerp(warm.startScale, warm.endScale, lifeRatio);
    float4 color = lerp(warm.startColor, warm.endColor, lifeRatio);

    // ②' 拡張Paramモジュール: スケール脈動・色の明滅（粒子ごとの経過時間 hot.currentTime を位相に使う）
    static const float kTwoPi = 6.28318530718f;
    if (g_ExtParams.pulseEnable != 0)
    {
        scale *= 1.0f + g_ExtParams.pulseAmplitude * sin(g_ExtParams.pulseFrequency * kTwoPi * hot.currentTime);
    }
    if (g_ExtParams.flickerEnable != 0)
    {
        // slot をシードにした位相オフセットで、粒子ごとに明滅がずれるようにする
        float phase = Hash11(slot) * kTwoPi;
        color.rgb *= 1.0f + g_ExtParams.flickerIntensity * sin(g_ExtParams.flickerSpeed * kTwoPi * hot.currentTime + phase);
    }
    // 発光: RGBのみ1.0超へ持ち上げて Bloom を乗せる（アルファ＝透過の形は保つ）
    if (g_ExtParams.emissiveEnable != 0)
    {
        color.rgb *= g_ExtParams.emissiveIntensity;
    }

    // ③ モデルローカルの頂点位置
    float3 localPos = input.position.xyz;

    // ④ Z 軸回転角をスカラーで取り出し
    float angle = hot.rotate;

    // ④' 拡張Paramモジュール: 速度方向へ引き伸ばす（火花・スピード線）
    // 速度をビルボード平面(right/up)へ投影して画面上の向きを求め、
    // ローカルY軸をその向きへ合わせてから Y だけ伸ばす。
    // 有効時は hot.rotate による回転を上書きする（速度を向くべきなので併用しない）。
    if (g_ExtParams.stretchEnable != 0)
    {
        float3 camRight = g_PerView.billboardMatrix[0].xyz;
        float3 camUp    = g_PerView.billboardMatrix[1].xyz;
        float2 vScreen  = float2(dot(hot.velocity, camRight), dot(hot.velocity, camUp));

        if (dot(vScreen, vScreen) > 1e-8f)
        {
            // ローカル +Y を速度方向へ向ける（atan2 は +X 基準なので π/2 引く）
            angle = atan2(vScreen.y, vScreen.x) - 1.57079632679f;

            float speed = length(hot.velocity);
            scale.y *= min(1.0f + speed * g_ExtParams.stretchScale, g_ExtParams.stretchMax);
        }
    }

    float cosA = cos(angle);
    float sinA = sin(angle);

    // ⑤ 2D 回転 (XY 平面)
    localPos.xy = float2(
        localPos.x * cosA - localPos.y * sinA,
        localPos.x * sinA + localPos.y * cosA
    );

    // ⑥ ワールド行列の構築
    // ビルボードはエミッタ単位のuniform（PerView）で判定＝切替が既存粒子含め即反映される
    float4x4 worldMatrix;
    if (g_PerView.isBillboard == 1)
    {
        // ビルボード行列にスケールだけ適用
        worldMatrix = g_PerView.billboardMatrix;
        worldMatrix[0] *= scale.x;
        worldMatrix[1] *= scale.y;
        worldMatrix[2] *= scale.z;
    }
    else
    {
        // 通常のスケール → 回転 合成
        float4x4 scaleMat = float4x4(
            float4(scale.x, 0, 0, 0),
            float4(0, scale.y, 0, 0),
            float4(0, 0, scale.z, 0),
            float4(0, 0, 0, 1)
        );
        float4x4 rotation = float4x4(
            float4(cosA, -sinA, 0, 0),
            float4(sinA, cosA, 0, 0),
            float4(0, 0, 1, 0),
            float4(0, 0, 0, 1)
        );
        worldMatrix = mul(rotation, scaleMat);
    }

    // ⑦ 平行移動
    float4 worldPos = mul(float4(localPos, 1.0f), worldMatrix);
    worldPos.xyz += hot.translate;

    // ⑧ ビュー・プロジェクション変換
    output.position = mul(worldPos, g_PerView.viewProjection);

    // ⑨ テクスチャ座標・色の受け渡し
    float2 uv = input.texcoord;

    if (g_ExtParams.uvScrollEnable != 0)
    {
        uv += g_ExtParams.uvScrollSpeed * hot.currentTime;
    }

    // フリップブック: アトラスを cols×rows のセルに区切ってコマ送りする。
    // UVスクロールと併用した場合でもセルからはみ出さないよう frac で丸めてから割り当てる。
    if (g_ExtParams.flipbookEnable != 0)
    {
        uint cols  = max(g_ExtParams.flipbookCols, 1u);
        uint rows  = max(g_ExtParams.flipbookRows, 1u);
        uint total = cols * rows;

        float frameF;
        if (g_ExtParams.flipbookFps > 0.0001f)
        {
            // 固定フレームレート再生（末尾までいったらループ）
            frameF = hot.currentTime * g_ExtParams.flipbookFps;
        }
        else
        {
            // 寿命全体でちょうど1周（爆発の一発再生）
            frameF = lifeRatio * (float) total;
        }

        uint frame = (uint) max(frameF, 0.0f) % total;
        uint cx = frame % cols;
        uint cy = frame / cols;

        float2 cellSize = float2(1.0f / (float) cols, 1.0f / (float) rows);
        uv = frac(uv) * cellSize + float2((float) cx, (float) cy) * cellSize;
    }

    output.texcoord = uv;
    output.color = color;

    return output;
}

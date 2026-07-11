#include "GPUParticle.hlsli"

// SoA: hot(t0) と warm(t1) を読み、scale/color は lifeRatio で lerp 導出する
// （Update CS / SpawnTrail CS と同一式）。cold はトレイル生成専用なので VS は読まない。
// インダイレクト描画: instanceID は「生存粒子の通し番号」なので DrawList(t2) で実スロットへ変換する。
// 描画インスタンス数は生存数ぶんだけなので、ここに来る時点で必ず生存＝isActive カリング不要。
StructuredBuffer<ParticleHot>  g_Hot  : register(t0);
StructuredBuffer<ParticleWarm> g_Warm : register(t1);
StructuredBuffer<uint>         g_DrawList : register(t2);
ConstantBuffer<PerView> g_PerView : register(b0);

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

    // ③ モデルローカルの頂点位置
    float3 localPos = input.position.xyz;

    // ④ Z 軸回転角をスカラーで取り出し
    float angle = hot.rotate;
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
    output.texcoord = input.texcoord;
    output.color = color;

    return output;
}

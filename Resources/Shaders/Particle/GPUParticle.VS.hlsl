#include "GPUParticle.hlsli"

StructuredBuffer<Particle> g_Particles : register(t0);
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

    // ① パーティクルパラメータ取得
    Particle particle = g_Particles[instanceID];
    if (particle.isActive == 0)
    {   //カリング
        VertexShaderOutput o;
        o.position = float4(0, 0, 0, 0);
        return o;
    }
    // ② モデルローカルの頂点位置
    float3 localPos = input.position.xyz;

    // ③ Z 軸回転角をスカラーで取り出し
    float angle = particle.rotate;
    float cosA = cos(angle);
    float sinA = sin(angle);

    // ④ 2D 回転 (XY 平面)
    localPos.xy = float2(
        localPos.x * cosA - localPos.y * sinA,
        localPos.x * sinA + localPos.y * cosA
    );

    // ⑤ ワールド行列の構築
    float4x4 worldMatrix;
    if (particle.isBillboard == 1)
    {
        // ビルボード行列にスケールだけ適用
        worldMatrix = g_PerView.billboardMatrix;
        worldMatrix[0] *= particle.scale.x;
        worldMatrix[1] *= particle.scale.y;
        worldMatrix[2] *= particle.scale.z;
    }
    else
    {
        // 通常のスケール → 回転 合成
        float4x4 scale = float4x4(
            float4(particle.scale.x, 0, 0, 0),
            float4(0, particle.scale.y, 0, 0),
            float4(0, 0, particle.scale.z, 0),
            float4(0, 0, 0, 1)
        );
        float4x4 rotation = float4x4(
            float4(cosA, -sinA, 0, 0),
            float4(sinA, cosA, 0, 0),
            float4(0, 0, 1, 0),
            float4(0, 0, 0, 1)
        );
        worldMatrix = mul(rotation, scale);
    }

    // ⑥ 平行移動
    float4 worldPos = mul(float4(localPos, 1.0f), worldMatrix);
    worldPos.xyz += particle.translate;

    // ⑦ ビュー・プロジェクション変換
    output.position = mul(worldPos, g_PerView.viewProjection);

    // ⑧ テクスチャ座標・色の受け渡し
    output.texcoord = input.texcoord;
    output.color = particle.color;

    return output;
}

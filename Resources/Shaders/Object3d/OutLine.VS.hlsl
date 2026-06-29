// =============================================================================
// インバートハル（背面押し出し）輪郭線 VS
//   頂点を法線方向へ押し出した「一回り大きいシェル」を生成する。
//   このシェルを前面カリング(背面のみ描画)で描くと、シルエットの外側だけが
//   残り、オブジェクトを縁取るインク線になる。
//   押し出し量にクリップ w(≒ビュー深度) を掛けることで、距離に依らず
//   ほぼ一定の太さ(スクリーン基準)になる。
// =============================================================================

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct Camera
{
    float3   worldPosition;
    float4x4 viewProjection;
};

struct Outline
{
    float4 color;             // 線の色
    float  width;             // 押し出し量(スクリーン基準)
    int    enable;            // 0=無効
    int    useDistanceFade;   // 距離フェード有効
    float  distanceFadeStart; // フェード開始(ビュー空間距離)。ここから線が細くなり始める
    float  distanceFadeEnd;   // フェード終了。これより遠いと太さ0(真っ黒化防止)
    float2 _pad;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<Outline>              gOutline              : register(b1);
ConstantBuffer<Camera>               gCamera               : register(b3);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal   : NORMAL0;
};

struct OutlineVSOutput
{
    float4 position : SV_POSITION;
};

OutlineVSOutput main(VertexShaderInput input)
{
    OutlineVSOutput output;

    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    float3 worldNormal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));

    // まずクリップ座標を求め、その w でスケール(距離に依らず一定太さ)。
    float4 clip = mul(worldPos, gCamera.viewProjection);

    // 距離フェード：遠ざかると太さを徐々に 0 へ。
    // スクリーン基準の一定太さのままだと、小さく映ったオブジェクトを線が覆って真っ黒になる。
    // clip.w はビュー空間の深度(≒カメラからの距離)。
    float distScale = 1.0f;
    if (gOutline.useDistanceFade != 0 && gOutline.distanceFadeEnd > gOutline.distanceFadeStart)
    {
        distScale = saturate(1.0f - (clip.w - gOutline.distanceFadeStart)
                                  / (gOutline.distanceFadeEnd - gOutline.distanceFadeStart));
    }

    worldPos.xyz += worldNormal * gOutline.width * clip.w * distScale;

    output.position = mul(worldPos, gCamera.viewProjection);
    return output;
}

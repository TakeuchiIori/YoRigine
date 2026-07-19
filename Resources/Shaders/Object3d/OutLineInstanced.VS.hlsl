// =============================================================================
// インバートハル（背面押し出し）輪郭線 VS（インスタンス描画版）
//   OutLine.VS.hlsl と同じく頂点を法線方向へ押し出してシェルを作るが、
//   ワールド/法線行列を per-instance の StructuredBuffer から取得する。
//   ModelManipulator 等でインスタンス描画される静的オブジェクトに輪郭を付ける。
// =============================================================================

// Per-instance データ (CPU側 InstancedObject3d::InstanceData / Object3dInstanced.VS と一致)
struct InstanceData
{
    float4x4 WVP;                  // 64
    float4x4 World;                // 64
    float4x4 WorldInverseTranspose;// 64
    float4   color;                // 16
    float4x4 uvTransform;          // 64
    float    stochasticStrength;   //  4
    float    outlineMask;          //  4  (1=線あり, 0=線なし。地面等を除外する)
    float    ditherFade;           //  4
    float    _pad2;                //  4
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
    float  distanceFadeStart; // フェード開始(ビュー空間距離)
    float  distanceFadeEnd;   // フェード終了。これより遠いと太さ0
    float2 _pad;
};

StructuredBuffer<InstanceData> gInstances : register(t3);
ConstantBuffer<Outline>        gOutline   : register(b1);
ConstantBuffer<Camera>         gCamera    : register(b3);

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

OutlineVSOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    OutlineVSOutput output;

    InstanceData inst = gInstances[instanceID];

    float4 worldPos = mul(input.position, inst.World);
    float3 worldNormal = normalize(mul(input.normal, (float3x3) inst.WorldInverseTranspose));

    // クリップ w(≒ビュー深度) でスケールして距離に依らず一定太さにする。
    float4 clip = mul(worldPos, gCamera.viewProjection);

    // 距離フェード：遠ざかると太さを徐々に 0 へ（小さく映ったオブジェクトの真っ黒化防止）。
    float distScale = 1.0f;
    if (gOutline.useDistanceFade != 0 && gOutline.distanceFadeEnd > gOutline.distanceFadeStart)
    {
        distScale = saturate(1.0f - (clip.w - gOutline.distanceFadeStart)
                                  / (gOutline.distanceFadeEnd - gOutline.distanceFadeStart));
    }

    // outlineMask=0 のインスタンス(地面など)は押し出さない＝輪郭が出ない。
    worldPos.xyz += worldNormal * gOutline.width * clip.w * distScale * inst.outlineMask;

    output.position = mul(worldPos, gCamera.viewProjection);
    return output;
}

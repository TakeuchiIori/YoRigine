
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
    float3 worldPosition : POSITION0;  // Object3d と同名・同セマンティクス
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
// メッシュ個別パラメータ
//--------------------------------------------------

// Trail 用
struct MeshTrailParams
{
    float4 colorInner;         // 先側の色
    float4 colorOuter;         // 根本側の色
    float softness;            // エッジフェード幅
    float glowPower;           // 発光強度
    float distortion;          // UV揺らぎ強度
    float time;           // 累積時間
};

// LightVolume 用
struct LightVolumeParams
{
    float4 color;             // 光の色 (a=強度スケール)
    float edgeFade;           // エッジのソフトフェード幅
    float depthFade;          // カメラ近接フェード距離
    float noiseTiling;        // ノイズUVタイリング
    float noiseStrength;      // ノイズ強度
    float time;
};

//--------------------------------------------------
// テクスチャ / サンプラー
//--------------------------------------------------
Texture2D    gTexNoise     : register(t0); // Trail歪み / Volume揺らぎ用ノイズ
Texture2D    gTexRamp      : register(t1); // Trailグラデーション LUT
SamplerState gSampler      : register(s0); // Linear Wrap  (Object3dと同名)
SamplerState gSamplerClamp : register(s1); // Linear Clamp

//--------------------------------------------------
// ユーティリティ
//--------------------------------------------------

// 0-1 の両端をソフトにフェードさせる
float EdgeFade(float t, float fadeWidth)
{
    float lo = smoothstep(0.0, fadeWidth, t);
    float hi = smoothstep(1.0, 1.0 - fadeWidth, t);
    return lo * hi;
}

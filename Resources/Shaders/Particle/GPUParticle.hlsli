static const uint kMaxParticles = 65536; // 1エミッタあたりの最大粒子数（バッファ・描画・ディスパッチの上限）。C++ 側 GPUParticle::kMaxParticles と必ず一致させること
static const uint kParticlesPerThread = 128; // 1スレッドが処理するパーティクル数。C++ 側 GPUParticle::kParticlesPerThread と一致させること

// エミッター形状の種類
static const uint EMITTER_SHAPE_SPHERE = 0;
static const uint EMITTER_SHAPE_BOX = 1;
static const uint EMITTER_SHAPE_TRIANGLE = 2;
static const uint EMITTER_SHAPE_CONE = 3;
static const uint EMITTER_SHAPE_MESH = 4;
static const uint EMITTER_SHAPE_RING = 5;
static const uint EMITTER_SHAPE_LINE = 6;

// メッシュ放出モード
static const uint MESH_EMIT_MODE_SURFACE = 0;
static const uint MESH_EMIT_MODE_VOLUME = 1;
static const uint MESH_EMIT_MODE_EDGE = 2;

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};


struct ChildParticleParams
{
    uint isTrail;
    uint inheritScale;
    float minDistance;
    float lifeTime;
    int emissionCount;
    float startScale;
    float endScale;
    float pad0;
};

// ─────────────────────────────────────────────────────────────────────────
// SoA レイアウト（AoS 176B から hot/warm/cold の3バッファへ分割）
//   scale / color は保存せず VS で startScale/endScale・startColor/endColor から
//   lifeRatio でlerp導出する（Update CS のlerpと同一式）。isBillboard は PerView(uniform)。
//   C++ 側 GPUParticle::ParticleHotGPU / ParticleWarmGPU / ParticleColdGPU と厳密一致。
// ─────────────────────────────────────────────────────────────────────────

// Hot: 毎フレーム Update が書き換える + VS も読む
struct ParticleHot
{
    float3 translate;   float rotate;        // 16
    float3 velocity;    float lifeTime;      // 16
    float  currentTime; uint  isActive; float2 pad; // 16
};  // 48 bytes

// Warm: Emit 時に確定・寿命中不変。VS が scale/color 導出に読む
struct ParticleWarm
{
    float3 startScale; float pad0;   // 16
    float3 endScale;   float pad1;   // 16
    float4 startColor;               // 16
    float4 endColor;                 // 16
};  // 64 bytes

// Cold: トレイル生成のみが使う（Update/描画は触らない）
struct ParticleCold
{
    float3 lastTranslate; uint isParent;  // 16
};  // 16 bytes


// パーティクルの初期パラメータ設定
struct ParticleParameters
{
    // 生存時間
    float lifeTime;
    float lifeTimeVariance;
    float2 pad0;
    
    // スケール
    float3 startScale;
    float pad1;
    float3 startScaleVariance;
    float pad2;
    float3 endScale;
    float pad3;
    float3 endScaleVariance;
    float pad4;
    
    // 回転
    float rotation;
    float rotationVariance;
    float rotationSpeed;
    float rotationSpeedVariance;
    
    // 速度
    float3 velocity;
    float pad5;
    float3 velocityVariance;
    float pad6;
    
    // 色
    float4 startColor;
    float4 startColorVariance;
    float4 endColor;
    float4 endColorVariance;
    
    // 物理
    float gravity;
    
    // ビルボード設定
    uint isBillboard;
    float2 pad7;
    
    // 子パーティクル設定
    ChildParticleParams child;
};

struct PerView
{
    float4x4 viewProjection;
    float4x4 billboardMatrix;
    uint isBillboard;   // エミッタ単位のビルボード ON/OFF
    float3 pad;
};

struct ParticleStats
{
    uint activeCount;
    uint freeCount;
    uint maxParticles;
};

struct EmitterCommon
{
    uint emitterShape;
};

struct EmitterBox
{
    float3 translate;
    float3 size; // width, height, depth
    float count;
    float emitInterval;
    float intervalTime;
    int isEmit;
};

struct EmitterSphere
{
    float3 translate;
    float radius;
    float count;
    float emitInterval;
    float intervalTime;
    int isEmit;
};

struct EmitterTriangle
{
    float3 v1;
    float3 v2;
    float3 v3;
    float3 translate;
    float count;
    float emitInterval;
    float intervalTime;
    int isEmit;
};

struct EmitterCone
{
    float3 translate;
    float3 direction; // 正規化された方向ベクトル
    float radius; // 底面の半径
    float height; // 円錐の高さ
    float count;
    float emitInterval;
    float intervalTime;
    int isEmit;
};

/// <summary>
/// リングエミッター（衝撃波の輪・魔法陣向け）
/// C++ 側 YGpuEmitter::EmitterRingData と同一レイアウト (64 bytes)
/// </summary>
struct EmitterRing
{
    float3 translate;
    float  pad0;         // 16
    float3 normal;       // リング面の法線
    float  outerRadius;  // 32
    float  innerRadius;
    float  count;
    float  emitInterval;
    float  intervalTime; // 48
    uint   isEmit;
    float3 pad1;         // 64
};

/// <summary>
/// ラインエミッター（レーザー・ビームの線状発生向け）
/// C++ 側 YGpuEmitter::EmitterLineData と同一レイアウト (48 bytes)
/// </summary>
struct EmitterLine
{
    float3 start;
    float  pad0;         // 16
    float3 end;
    float  pad1;         // 32
    float  count;
    float  emitInterval;
    float  intervalTime;
    uint   isEmit;       // 48
};

/// <summary>
/// メッシュエミッター
/// </summary>
struct EmitterMesh
{
    float3 translate;
    float pad0; // 16
    float3 scale;
    float pad1; // 32
    float4 rotation; // 48

    float count; // 52
    float emitInterval; // 56
    float intervalTime; // 60
    uint isEmit; // 64

    uint emitMode; // 68
    uint triangleCount; // 72
    float pad2; // 76
    float pad3; // 80 ← ★これが必要！
};



/// <summary>
/// メッシュ三角形データ
/// </summary>
struct MeshTriangle
{
    float3 v0;
    float3 v1;
    float3 v2;
    float3 normal;
    float area;
    uint activeEdges;
};

// ──── 拡張Paramモジュール（任意ON/OFFの演出） ───────────────────────────────
// エミッタ単位の共有CBV。VS(YGpuParticle.VS.hlsl)が b1、
// Update CS(UpdateParticle.CS.hlsl)が b2 で同じ内容を読む。
//   VS が使う: uvScroll / pulse / flicker / stretch（見た目）
//   CS が使う: drag / bounce（速度・位置の物理更新）
// C++ 側 YGpuParticle::ParticleExtParameters と厳密に同一レイアウト (96 bytes)。
struct ParticleExtParams
{
    float2 uvScrollSpeed;
    uint   uvScrollEnable;
    float  pad0;              // 16

    float  pulseAmplitude;
    float  pulseFrequency;
    uint   pulseEnable;
    float  pad1;              // 32

    float  flickerSpeed;
    float  flickerIntensity;
    uint   flickerEnable;
    float  pad2;              // 48

    float  dragCoefficient;
    uint   dragEnable;
    float2 pad3;              // 64

    float  stretchScale;
    float  stretchMax;
    uint   stretchEnable;
    float  pad4;              // 80

    float  bounceGroundY;
    float  bounceRestitution;
    float  bounceFriction;
    uint   bounceEnable;      // 96

    float  emissiveIntensity;
    uint   emissiveEnable;
    float2 pad5;              // 112

    uint   flipbookCols;
    uint   flipbookRows;
    float  flipbookFps;       // 0以下なら寿命全体でちょうど1周する
    uint   flipbookEnable;    // 128
};
// ────────────────────────────────────────────────────────────────────────────

struct PerFrame
{
    float time;
    float deltaTime;
    uint  forceFieldCount; // GPU フォースフィールドの有効数。0 = フィールド無し
    uint  noiseFieldCount; // GPU ノイズフィールド(Curl/Turbulence/Vortex)の有効数。0 = 無し

    uint  accelerationFieldCount; // GPU アクセラレーションフィールドの有効数。0 = 無し
    float3 pad;
};

// ──── GPU フォースフィールド ────────────────────────────────────────────────
static const uint FIELD_SHAPE_SPHERE = 0;
static const uint FIELD_SHAPE_AABB   = 1;
static const uint FIELD_MODE_DIRECTIONAL = 0;
static const uint FIELD_MODE_CONVERGE    = 1;
static const uint FIELD_MODE_REPEL       = 2;

// C++ 側 GPUEmitter::AccelerationForGPU と厳密に同一レイアウト (64 bytes)
// ForceFieldのDirectionalAccelよりも単純な、範囲内一定方向の加速度のみを持つ軽量フィールド。
struct GpuAccelerationField {
    uint   shape;         // FIELD_SHAPE_*
    float3 center;        // 16
    float3 halfExtents;   // AABB用
    float  radius;        // 32 (Sphere用)
    float3 direction;
    float  strength;      // 48
    float  falloff;
    uint   isEnable;
    float2 pad;           // 64
};

// C++ 側 GPUEmitter::ForceFieldForGPU と厳密に同一レイアウト (96 bytes)
struct GpuForceField {
    uint   shape;         // FIELD_SHAPE_*
    float3 center;        // 16
    float3 halfExtents;   // AABB 用
    float  radius;        // 32
    uint   mode;          // FIELD_MODE_*
    float3 direction;     // DirectionalAccel 用  // 48
    float  strength;
    float  falloff;
    float  spiralStrengthMin;
    float  spiralStrengthMax; // 64
    float  randomAxisBlend;
    float  orbitHoldRatio;
    float  approachVariance;
    float  maxSpeed;          // 80
    float  killRadius;
    uint   isEnable;
    float2 pad2;              // 96
};
// ────────────────────────────────────────────────────────────────────────────

// ──── GPU ノイズフィールド (Curl / Turbulence / Vortex) ─────────────────────
static const uint NOISE_TYPE_CURL       = 0;
static const uint NOISE_TYPE_TURBULENCE = 1;
static const uint NOISE_TYPE_VORTEX     = 2;

// C++ 側 GPUEmitter::NoiseForGPU と厳密に同一レイアウト (96 bytes)
struct GpuNoiseField {
    uint   type;         // NOISE_TYPE_*
    float  frequency;
    float  amplitude;
    uint   octaves;       // 16 (Turbulenceのみ使用)

    float  lacunarity;
    float  gain;
    float2 pad0;          // 32

    float3 scrollSpeed;  float pad1; // 48 (ノイズ空間を時間で流す速度)
    float3 axis;         float pad2; // 64 (Vortexの回転軸)
    float3 center;       float radius; // 80 (Vortex/Turbulenceの中心・減衰半径。0=無限)

    uint   isEnable;
    float3 pad3;          // 96
};
// ────────────────────────────────────────────────────────────────────────────

// ──── ハッシュ補助関数 ────────────────────────────────────────────────────
uint Hash11u(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return x;
}
float Hash11(uint x) { return float(Hash11u(x)) / 4294967295.0f; }
float3 HashToUnitVector(uint seed) {
    float theta = Hash11(seed ^ 0x9e3779b9u) * 6.28318f;
    float cosP  = Hash11(seed ^ 0x517cc1b7u) * 2.0f - 1.0f;
    float sinP  = sqrt(max(0.0f, 1.0f - cosP * cosP));
    return float3(sinP * cos(theta), cosP, sinP * sin(theta));
}
// ────────────────────────────────────────────────────────────────────────────


//=============================================================================
// ヘルパー関数
//=============================================================================

/// <summary>
/// クォータニオンによる回転を適用
/// </summary>
float3 RotateByQuaternion(float3 v, float4 q)
{
    // q = (x, y, z, w)
    // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    float3 qxyz = q.xyz;
    float qw = q.w;
    
    float3 temp = cross(qxyz, v) + qw * v;
    return v + 2.0f * cross(qxyz, temp);
}

/// <summary>
/// 完全なトランスフォーム適用
/// </summary>
float3 TransformPoint(float3 position, float3 scale, float4 rotation, float3 translation)
{
    // 1. スケール適用
    float3 scaled = position * scale;
    
    // 2. 回転適用
    float3 rotated = RotateByQuaternion(scaled, rotation);
    
    // 3. 平行移動適用
    return rotated + translation;
}

/// <summary>
/// ベクトルを回転のみ適用（方向ベクトル用）
/// </summary>
float3 TransformDirection(float3 direction, float4 rotation)
{
    return normalize(RotateByQuaternion(direction, rotation));
}

/// <summary>
/// 重心座標で三角形内の点を計算
/// </summary>
float3 BarycentricInterpolation(float3 v0, float3 v1, float3 v2, float u, float v)
{
    float w = 1.0f - u - v;
    return v0 * w + v1 * u + v2 * v;
}

/// <summary>
/// 三角形の面積を計算
/// </summary>
float CalculateTriangleArea(float3 v0, float3 v1, float3 v2)
{
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    return length(cross(edge1, edge2)) * 0.5f;
}

/// <summary>
/// 三角形の法線を計算
/// </summary>
float3 CalculateTriangleNormal(float3 v0, float3 v1, float3 v2)
{
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    return normalize(cross(edge1, edge2));
}

/// <summary>
/// 三角形の中心を計算
/// </summary>
float3 CalculateTriangleCenter(float3 v0, float3 v1, float3 v2)
{
    return (v0 + v1 + v2) / 3.0f;
}

/// <summary>
/// ベクトルの長さの2乗を計算（距離比較用・高速）
/// </summary>
float LengthSquared(float3 v)
{
    return dot(v, v);
}

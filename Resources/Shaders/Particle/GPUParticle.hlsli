static const uint kMaxParticles = 500000; // 1024 × 4 = 4096個に増加
static const uint kParticlesPerThread = 128; // 1スレッドが処理するパーティクル数

// エミッター形状の種類
static const uint EMITTER_SHAPE_SPHERE = 0;
static const uint EMITTER_SHAPE_BOX = 1;
static const uint EMITTER_SHAPE_TRIANGLE = 2;
static const uint EMITTER_SHAPE_CONE = 3;
static const uint EMITTER_SHAPE_MESH = 4;

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

struct Particle
{
    float3 translate;
    float pad0;
    
    float3 lastTranslate;
    float pad1;
    
    float3 scale;
    float pad2;
    
    float3 startScale;
    float pad3;
    
    float3 endScale;
    float pad4;
    
    float rotate;
    float lifeTime;
    float currentTime;
    float pad5;
    
    float3 velocity;
    float pad6;

    float4 color;
    float4 startColor;
    float4 endColor;
    
    uint isParent;
    uint isBillboard;
    uint isActive;
    float pad7;
};


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

struct PerFrame
{
    // ゲームを起動してからの時間
    // パラメータいれるといい
    
    float time;
    float deltaTime;
};


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

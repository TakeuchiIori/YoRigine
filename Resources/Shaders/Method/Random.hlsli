// -----------------------------------------------------------------------------
// Random.hlsli  —  simple hash‑based random helpers (float/float2/float3)
//   * 関数名は既存プロジェクト互換で変更なし
//   * スカラー／ベクトル型のミスマッチを解消（-Wconversion 警告ゼロ）
//   * sin ハッシュ（軽量）を維持
// -----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// 1D helpers ------------------------------------------------------------------
// ----------------------------------------------------------------------------
// get a scalar random value from a 1d value
float rand1dTo1d(float value, float mutator = 0.546)
{
    // 0‑1 range
    float random = frac(sin(value + mutator) * 143758.5453);
    return random;
}

// get a 2d random value from a 1d value
float2 rand1dTo2d(float value)
{
    return float2(
        rand1dTo1d(value, 3.9812),
        rand1dTo1d(value, 7.1536)
    );
}

// get a 3d random value from a 1d value
float3 rand1dTo3d(float value)
{
    return float3(
        rand1dTo1d(value, 3.9812),
        rand1dTo1d(value, 7.1536),
        rand1dTo1d(value, 5.7241)
    );
}

// ----------------------------------------------------------------------------
// 2D helpers ------------------------------------------------------------------
// ----------------------------------------------------------------------------
// get a scalar random value from a 2d value
float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898, 78.233))
{
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

// get a 2d random value from a 2d value
float2 rand2dTo2d(float2 value)
{
    return float2(
        rand2dTo1d(value, float2(12.989, 78.233)),
        rand2dTo1d(value, float2(39.346, 11.135))
    );
}

// get a 3d random value from a 2d value
float3 rand2dTo3d(float2 value)
{
    return float3(
        rand2dTo1d(value, float2(12.989, 78.233)),
        rand2dTo1d(value, float2(39.346, 11.135)),
        rand2dTo1d(value, float2(73.156, 52.235))
    );
}

// ----------------------------------------------------------------------------
// 3D helpers ------------------------------------------------------------------
// ----------------------------------------------------------------------------
// get a scalar random value from a 3d value
float rand3dTo1d(float3 value, float3 dotDir = float3(12.9898, 78.233, 37.719))
{
    float3 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

// get a 2d random value from a 3d value
float2 rand3dTo2d(float3 value)
{
    return float2(
        rand3dTo1d(value, float3(12.989, 78.233, 37.719)),
        rand3dTo1d(value, float3(39.346, 11.135, 83.155))
    );
}

// get a 3d random value from a 3d value
float3 rand3dTo3d(float3 value)
{
    return float3(
        rand3dTo1d(value, float3(12.989, 78.233, 37.719)),
        rand3dTo1d(value, float3(39.346, 11.135, 83.155)),
        rand3dTo1d(value, float3(73.156, 52.235, 9.151))
    );
}


struct RandomGenerator
{
    float3 seed;

    // 1D output (float)
    float Generated1d()
    {
        float result = rand3dTo1d(seed);
        seed.x = result; // フィードバックで種を更新
        return result;
    }

    // 2D output (float2)
    float2 Generated2d()
    {
        float2 result = rand3dTo2d(seed);
        seed.xy = result;
        return result;
    }

    // 3D output (float3)
    float3 Generated3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }
};

// Sphere内のランダム位置を生成
float3 GenerateRandomPositionInSphere(inout RandomGenerator generator, float3 center, float radius)
{
    float3 randomOffset = (generator.Generated3d() * 2.0f - 1.0f);
    randomOffset = normalize(randomOffset) * pow(generator.Generated1d(), 1.0f / 3.0f) * radius;
    return center + randomOffset;
}

// Box内のランダム位置を生成
float3 GenerateRandomPositionInBox(inout RandomGenerator generator, float3 center, float3 size)
{
    float3 randomOffset = (generator.Generated3d() * 2.0f - 1.0f) * size * 0.5f;
    return center + randomOffset;
}

// Triangle内のランダム位置を生成（重心座標を使用）
float3 GenerateRandomPositionInTriangle(inout RandomGenerator generator, float3 v1, float3 v2, float3 v3)
{
    float r1 = generator.Generated1d();
    float r2 = generator.Generated1d();
    
    // 三角形の外に出ないようにする
    if (r1 + r2 > 1.0f)
    {
        r1 = 1.0f - r1;
        r2 = 1.0f - r2;
    }
    
    float r3 = 1.0f - r1 - r2;
    return r1 * v1 + r2 * v2 + r3 * v3;
}

// Cone内のランダム位置を生成
float3 GenerateRandomPositionInCone(inout RandomGenerator generator, float3 apex, float3 direction, float radius, float height)
{
    // 円錐の高さ方向のランダムな位置
    float t = pow(generator.Generated1d(), 1.0f / 3.0f); // 体積を考慮した分布
    float currentHeight = t * height;
    float currentRadius = t * radius;
    
    // 円形断面内のランダムな位置
    float angle = generator.Generated1d() * 2.0f * 3.14159f;
    float r = sqrt(generator.Generated1d()) * currentRadius;
    
    // 方向ベクトルに垂直な平面を作成
    float3 up = abs(direction.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(direction, up));
    up = cross(right, direction);
    
    float3 circlePos = r * (cos(angle) * right + sin(angle) * up);
    return apex + currentHeight * direction + circlePos;
}
/// <summary>
/// パーティクル用の空間分散シードを生成
/// </summary>
float3 GenerateParticleSeed(uint particleIndex, uint totalParticles, float time)
{
    // Fibonacci格子点を使った空間分散
    const float PHI = 1.618033988749895;
    const float PI = 3.14159265359;
    
    float normalizedIndex = float(particleIndex) / max(1.0, float(totalParticles));
    
    // 黄金角を使った回転
    float goldenAngle = 2.0 * PI / (PHI * PHI);
    float theta = goldenAngle * float(particleIndex);
    float z = 1.0 - 2.0 * normalizedIndex;
    float radius = sqrt(1.0 - z * z);
    
    // 3次元空間に分散した初期シード
    float3 spatialSeed = float3(
        radius * cos(theta),
        radius * sin(theta),
        z
    );
    
    // 時間要素を加える（各成分に異なる周波数）
    float3 timeSeed = float3(
        frac(sin(time * 12.9898 + float(particleIndex)) * 43758.5453),
        frac(sin(time * 78.2330 + float(particleIndex)) * 12345.6789),
        frac(sin(time * 37.7190 + float(particleIndex)) * 98765.4321)
    );
    
    // 空間シードと時間シードを組み合わせる
    return frac(spatialSeed * 0.5 + 0.5 + timeSeed);
}

/// <summary>
/// パラメータからランダムな値を生成
/// </summary>
float GetRandomValue(inout RandomGenerator generator, float baseValue, float variance)
{
    return baseValue + (generator.Generated1d() * 2.0f - 1.0f) * variance;
}

float3 GetRandomVector3(inout RandomGenerator generator, float3 baseValue, float3 variance)
{
    return baseValue + (generator.Generated3d() * 2.0f - 1.0f) * variance;
}

float4 GetRandomColor(inout RandomGenerator generator, float4 baseColor, float4 variance)
{
    return baseColor + (float4(generator.Generated3d(), generator.Generated1d()) * 2.0f - 1.0f) * variance;
}
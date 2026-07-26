#ifndef NOISE_LIB_HLSLI
#define NOISE_LIB_HLSLI

// ─────────────────────────────────────────────────────────────────────────
// GPU パーティクル用ノイズライブラリ (Curl / Turbulence(fBm) / Vortex)
// UpdateParticle.CS.hlsl から include される。
// ─────────────────────────────────────────────────────────────────────────

float3 Hash33(float3 p)
{
    p = float3(dot(p, float3(127.1, 311.7, 74.7)),
               dot(p, float3(269.5, 183.3, 246.1)),
               dot(p, float3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * frac(sin(p) * 43758.5453123);
}

// 3D 勾配ノイズ (Perlin 風)
float PerlinNoise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);

    float n000 = dot(Hash33(i + float3(0, 0, 0)), f - float3(0, 0, 0));
    float n100 = dot(Hash33(i + float3(1, 0, 0)), f - float3(1, 0, 0));
    float n010 = dot(Hash33(i + float3(0, 1, 0)), f - float3(0, 1, 0));
    float n110 = dot(Hash33(i + float3(1, 1, 0)), f - float3(1, 1, 0));
    float n001 = dot(Hash33(i + float3(0, 0, 1)), f - float3(0, 0, 1));
    float n101 = dot(Hash33(i + float3(1, 0, 1)), f - float3(1, 0, 1));
    float n011 = dot(Hash33(i + float3(0, 1, 1)), f - float3(0, 1, 1));
    float n111 = dot(Hash33(i + float3(1, 1, 1)), f - float3(1, 1, 1));

    float nx00 = lerp(n000, n100, u.x);
    float nx10 = lerp(n010, n110, u.x);
    float nx01 = lerp(n001, n101, u.x);
    float nx11 = lerp(n011, n111, u.x);
    float nxy0 = lerp(nx00, nx10, u.y);
    float nxy1 = lerp(nx01, nx11, u.y);
    return lerp(nxy0, nxy1, u.z);
}

// Curl noise: ポテンシャル場の有限差分から発散ゼロのベクトル場を作る（乱流の巻き込み表現向け）
float3 CurlNoise3D(float3 p, float epsilon)
{
    float3 dx = float3(epsilon, 0, 0);
    float3 dy = float3(0, epsilon, 0);
    float3 dz = float3(0, 0, epsilon);

    float x1 = PerlinNoise3D(p + dy) - PerlinNoise3D(p - dy);
    float x2 = PerlinNoise3D(p + dz) - PerlinNoise3D(p - dz);
    float y1 = PerlinNoise3D(p + dz) - PerlinNoise3D(p - dz);
    float y2 = PerlinNoise3D(p + dx) - PerlinNoise3D(p - dx);
    float z1 = PerlinNoise3D(p + dx) - PerlinNoise3D(p - dx);
    float z2 = PerlinNoise3D(p + dy) - PerlinNoise3D(p - dy);

    float3 curl = float3(x1 - x2, y1 - y2, z1 - z2) / (2.0 * epsilon);
    return curl;
}

// fBm Turbulence: オクターブを重ねてざらついた乱流感を出す
float3 TurbulenceNoise3D(float3 p, uint octaves, float lacunarity, float gain)
{
    float3 sum = float3(0, 0, 0);
    float  amp = 1.0;
    float3 pp = p;
    for (uint o = 0; o < octaves; o++)
    {
        sum += Hash33(floor(pp)) * PerlinNoise3D(pp) * amp;
        pp *= lacunarity;
        amp *= gain;
    }
    return sum;
}

// Vortex: 軸周りの接線方向へ力を発生させる
float3 VortexForce(float3 worldPos, float3 center, float3 axis, float radius)
{
    float3 toCenter = worldPos - center;
    float3 axisN = normalize(axis + 1e-6);
    float3 radial = toCenter - axisN * dot(toCenter, axisN); // 軸への投影を除いた半径方向
    float dist = length(radial);
    if (dist < 1e-4) return float3(0, 0, 0);

    float falloff = (radius > 0.0001) ? saturate(1.0 - dist / radius) : 1.0;
    float3 tangent = normalize(cross(axisN, radial));
    return tangent * falloff;
}

#endif

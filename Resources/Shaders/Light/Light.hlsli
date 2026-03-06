#define MAX_LIGHT_COUNT 10

struct DirectionalLightData
{
    float4 color;
    float3 direction;
    float intensity;
    int isEnableDirectionalLighting;
};

struct PointLightData
{
    float4 color;
    float3 position;
    float intensity;
    int isEnablePointLight;
    float radius;
    float decay;
    // 修正: 配列を展開し、隙間なくパックされるように個別のfloatにします
    float pad;
};

struct SpotLightData
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    int isEnableSpotLight;
};

struct PointLights
{
    PointLightData pointLights[MAX_LIGHT_COUNT];
    int numPointLights;
    // 修正: float配列[3]ではなく、float3を使用します
    float3 padding;
};

struct SpotLights
{
    SpotLightData spotLights[MAX_LIGHT_COUNT];
    int numSpotLights;
    // 修正: float配列[3]ではなく、float3を使用します
    float3 padding;
};
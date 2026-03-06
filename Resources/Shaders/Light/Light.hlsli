#define MAX_LIGHT_COUNT 10

struct DirectionalLightData
{
    float4 color;                       // ライトの色
    float3 direction;                   // ライトの向き
    float intensity;                    // 輝度
    int isEnableDirectionalLighting;    // ライトの有効フラグ
};


struct PointLightData
{
    float4 color;               // ライトの色
    float3 position;            // ライトの位置
    float intensity;            // 輝度
    int isEnablePointLight;     // ライトの有効フラグ
    float radius;               // ライトの届く最大距離
    float decay;                // 減衰率
};

struct SpotLightData
{
    float4 color;               // ライトの色
    float3 position;            // ライトの位置
    float intensity;            // 輝度
    float3 direction;           // ライトの向き
    float distance;             // ライトの届く最大距離
    float decay;                // 減衰率
    float cosAngle;             // cosで表現されたスポットライトの角度
    float cosFalloffStart;      // cosで表現されたスポットライトの減衰開始角度
    int isEnableSpotLight;      // ライトの有効フラグ
};

struct PointLights
{
    PointLightData pointLights[MAX_LIGHT_COUNT];    // ポイントライトの配列
    int numPointLights;                             // 使用するポイントライトの数
};

struct SpotLights
{
    SpotLightData spotLights[MAX_LIGHT_COUNT];      // スポットライトの配列
    int numSpotLights;                              // 使用するスポットライトの数
};
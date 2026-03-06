#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <cassert>

#include "Systems./Camera/Camera.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

class Object3dCommon;

namespace YoRigine {
    class LightManager final
    {
    public:
        static constexpr int kMaxPointLights = 10;
        static constexpr int kMaxSpotLights = 10;

        struct ShadowMapSettings {
            float shadowDistance = 1.0f;
            float orthoWidth = 150.0f;
            float orthoHeight = 150.0f;
            float nearZ = 1.0f;
            float farZ = 150.0f;
        };

        ///************************* GPU用の構造体（HLSLと完全一致）*************************///

        struct DirectionalLightData {
            Vector4  color;
            Vector3  direction;
            float    intensity;
            int32_t  isEnableDirectionalLighting;
        };

        struct PointLightData {
            Vector4  color;
            Vector3  position;
            float    intensity;
            int32_t  isEnablePointLight;
            float    radius;
            float    decay;
            float    padding;    // 48バイトに揃える
        };

        struct SpotLightData {
            Vector4  color;
            Vector3  position;
            float    intensity;
            Vector3  direction;
            float    distance;
            float    decay;
            float    cosAngle;
            float    cosFalloffStart;
            int32_t  isEnableSpotLight; // 64バイト、パディング不要
        };

        struct PointLightArray {
            PointLightData lights[kMaxPointLights];
            int32_t        count;
            float          padding[3];
        };

        struct SpotLightArray {
            SpotLightData lights[kMaxSpotLights];
            int32_t       count;
            float         padding[3];
        };

    private:
        struct ShadowMatrix {
            Matrix4x4 lightViewProjection;
        };

    public:
        ///************************* 基本関数 *************************///
        static LightManager* GetInstance();
        void Initialize();
        void UpdateShadowMatrix(Camera* camera);

        // 3Dオブジェクト用（全ライト）
        void SetCommandList(UINT directionalIndex,
            UINT pointIndex,
            UINT spotIndex);

        // パーティクル・スプライト用（平行光源のみ）
        void SetCommandListDirectionalOnly(UINT directionalIndex);

        ///************************* ポイントライト操作 *************************///
        int  AddPointLight(const PointLightData& light);
        void UpdatePointLight(int index, const PointLightData& light);
        void RemovePointLight(int index);
        int  GetPointLightCount() const { return pointLights_->count; }
        PointLightData& GetPointLight(int index) {
            assert(index >= 0 && index < pointLights_->count);
            return pointLights_->lights[index];
        }

        ///************************* スポットライト操作 *************************///
        int  AddSpotLight(const SpotLightData& light);
        void UpdateSpotLight(int index, const SpotLightData& light);
        void RemoveSpotLight(int index);
        int  GetSpotLightCount() const { return spotLights_->count; }
        SpotLightData& GetSpotLight(int index) {
            assert(index >= 0 && index < spotLights_->count);
            return spotLights_->lights[index];
        }

        ///************************* 平行光源アクセッサ *************************///
        const Vector4& GetDirectionalLightColor()     const { return directionalLight_->color; }
        void SetDirectionalLightColor(const Vector4& c) { directionalLight_->color = c; }
        const Vector3& GetDirectionalLightDirection() const { return directionalLight_->direction; }
        void SetDirectionalLightDirection(const Vector3& d) { directionalLight_->direction = d; }
        float GetDirectionalLightIntensity()          const { return directionalLight_->intensity; }
        void SetDirectionalLightIntensity(float v) { directionalLight_->intensity = v; }
        bool IsDirectionalLightEnabled()              const { return directionalLight_->isEnableDirectionalLighting != 0; }
        void SetDirectionalLightEnabled(bool v) { directionalLight_->isEnableDirectionalLighting = v; }

        ///************************* リソース取得 *************************///
        ID3D12Resource* GetDirectionalLightResource() const { return directionalLightResource_.Get(); }
        ID3D12Resource* GetShadowResource()           const { return shadowResource_.Get(); }

        ///************************* シャドウマップ設定 *************************///
        ShadowMapSettings& GetShadowMapSettings() { return shadowMapSettings_; }
        void SetShadowMapSettings(const ShadowMapSettings& s) { shadowMapSettings_ = s; }

        ///************************* ImGui *************************///
        void ShowLightingEditor();

    private:
        void CreateDirectionalLightResource();
        void CreatePointLightResource();
        void CreateSpotLightResource();
        void CreateShadowResource();

        LightManager() = default;
        ~LightManager() = default;
        LightManager(const LightManager&) = delete;
        LightManager& operator=(const LightManager&) = delete;
        LightManager(LightManager&&) = delete;
        LightManager& operator=(LightManager&&) = delete;

        ///************************* メンバ変数 *************************///
        Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
        DirectionalLightData* directionalLight_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> pointLightsResource_;
        PointLightArray* pointLights_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> spotLightsResource_;
        SpotLightArray* spotLights_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> shadowResource_;
        ShadowMatrix* shadow_ = nullptr;

        Object3dCommon* object3dCommon_ = nullptr;
        Camera* camera_ = nullptr;
        ShadowMapSettings shadowMapSettings_;
    };
}
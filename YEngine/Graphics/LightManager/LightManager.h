#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

#include "Systems./Camera/Camera.h"
#include "DsvManager.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

class Object3dCommon;

namespace YoRigine {
    class JsonManager;
    class LightManager final
    {
    public:
        static constexpr int kMaxPointLights = 10;
        static constexpr int kMaxSpotLights = 10;
        // カスケードシャドウの枚数（DsvManager と一致させること）
        static constexpr int kCascadeCount = DsvManager::kShadowCascadeCount;

        struct ShadowMapSettings {
            float shadowDistance = 1.0f;
            float orthoWidth = 150.0f;   // 最遠カスケードの覆う幅（近いカスケードは splitRatio 倍で縮小）
            float orthoHeight = 150.0f;
            float nearZ = 1.0f;
            float farZ = 150.0f;
            float cascadeSplitRatio = 0.5f; // 隣接カスケード間のサイズ比 (0<r<1)。小さいほど近距離を高精細化
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

        struct PointLightInstance {
            std::string name;
            PointLightData data;
        };
        struct SpotLightInstance {
            std::string name;
            SpotLightData data;
        };

    private:
        // 影パス（VS）用：1 カスケード分のライト VP
        struct ShadowMatrix {
            Matrix4x4 lightViewProjection;
        };
        // カラーパス（PS）用：全カスケードのライト VP 配列
        struct CascadeMatrices {
            Matrix4x4 lightViewProjection[kCascadeCount];
        };

    public:
        ///************************* 基本関数 *************************///
        static LightManager* GetInstance();
        void Initialize();
        void TransferData();
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

        void AddPointLight(const std::string& name, const PointLightData& data);
        PointLightData* GetPointLight(const std::string& name);
        void RemovePointLight(const std::string& name);

        ///************************* スポットライト操作 *************************///
        int  AddSpotLight(const SpotLightData& light);
        void UpdateSpotLight(int index, const SpotLightData& light);
        void RemoveSpotLight(int index);
        int  GetSpotLightCount() const { return spotLights_->count; }
        SpotLightData& GetSpotLight(int index) {
            assert(index >= 0 && index < spotLights_->count);
            return spotLights_->lights[index];
        }

        void AddSpotLight(const std::string& name, const SpotLightData& data);
        SpotLightData* GetSpotLight(const std::string& name);
        void RemoveSpotLight(const std::string& name);

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
        // 影パス用：現在描画中のカスケードの単一行列 CB（VS が読む）
        ID3D12Resource* GetShadowResource()           const { return shadowCascadeResource_[currentCascade_].Get(); }
        // カラーパス用：全カスケード行列配列 CB（PS が読む）
        ID3D12Resource* GetCascadeResource()          const { return cascadeResource_.Get(); }

        ///************************* カスケード制御 *************************///
        // 影パスのループで、これから描くカスケードを指定してから DrawShadow を呼ぶ
        void SetCurrentCascade(uint32_t index) { currentCascade_ = index; }
        uint32_t GetCurrentCascade() const { return currentCascade_; }

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
        ~LightManager();
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

        // --- 管理用コンテナ ---
        std::vector<PointLightInstance> pointLightInstances_;
        std::unordered_map<std::string, size_t> pointLightIndexMap_;

        std::vector<SpotLightInstance> spotLightInstances_;
        std::unordered_map<std::string, size_t> spotLightIndexMap_;

        // 影パス用：カスケードごとの単一行列 CB（VS が読む。現在カスケードを GetShadowResource で返す）
        Microsoft::WRL::ComPtr<ID3D12Resource> shadowCascadeResource_[kCascadeCount];
        ShadowMatrix* shadowCascadeMapped_[kCascadeCount] = {};
        uint32_t currentCascade_ = 0;

        // カラーパス用：全カスケードの行列配列 CB（PS が読む）
        Microsoft::WRL::ComPtr<ID3D12Resource> cascadeResource_;
        CascadeMatrices* cascade_ = nullptr;

        Object3dCommon* object3dCommon_ = nullptr;
        Camera* camera_ = nullptr;
        ShadowMapSettings shadowMapSettings_;

        // ShadowMapSettings の JSON 永続化用
        std::unique_ptr<JsonManager> shadowJson_;
    };
}
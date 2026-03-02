#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>

// Engine
#include "Systems./Camera/Camera.h"

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"


class DirectXCommon;
class Object3dCommon;
/// <summary>
/// ライト管理クラス
/// </summary>
namespace YoRigine {
	class LightManager final
	{
	public:
		struct ShadowMapSettings {
			float shadowDistance = 1.0f;
			float orthoWidth = 150.0f;
			float orthoHeight = 150.0f;
			float nearZ = 1.0f;
			float farZ = 150.0f;
		};
		///************************* 基本関数 *************************///
		static LightManager* GetInstance();
		void Initialize();

		void UpdateShadowMatrix(Camera* camera);
		void SetCommandList(UINT directionalIndex = 3,UINT pointIndex = 5,UINT spotIndex = 6);

	public:
		///************************* ライトのパラメーラー設定 *************************///

		void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity, bool enable);
		void SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay, bool enable);
		void ShowLightingEditor();

		/// <summary>
		/// リソースの取得関数
		/// </summary>
		ID3D12Resource* GetDirectionalLightResource() const { return directionalLightResource_.Get(); }
		ID3D12Resource* GetPointLightResource() const { return pointLightResource_.Get(); }
		ID3D12Resource* GetShadowResource() const { return shadowResource_.Get(); }

	private:
		///************************* 内部処理 *************************///
		void CreateDirectionalLightResource();
		void CreatePointLightResource();
		void CreateSpotLightResource();
		void CreateShadowResource();

	public:
		///************************* アクセッサ *************************///

		// 平行光源
		const Vector4& GetDirectionalLightColor() const { return directionalLight_->color; }
		void SetDirectionalLightColor(const Vector4& color) { directionalLight_->color = color; }
		const Vector3& GetDirectionalLightDirection() const { return directionalLight_->direction; }
		void SetDirectionalLightDirection(const Vector3& direction) { directionalLight_->direction = direction; }
		float GetDirectionalLightIntensity() const { return directionalLight_->intensity; }
		void SetDirectionalLightIntensity(float intensity) { directionalLight_->intensity = intensity; }
		bool IsDirectionalLightEnabled() const { return directionalLight_->enableDirectionalLight != 0; }
		void SetDirectionalLightEnabled(bool enable) { directionalLight_->enableDirectionalLight = enable; }

		// ポイントライト
		const Vector4& GetPointLightColor() const { return pointLight_->color; }
		void SetPointLightColor(const Vector4& color) { pointLight_->color = color; }
		const Vector3& GetPointLightPosition() const { return pointLight_->position; }
		void SetPointLightPosition(const Vector3& position) { pointLight_->position = position; }
		float GetPointLightIntensity() const { return pointLight_->intensity; }
		void SetPointLightIntensity(float intensity) { pointLight_->intensity = intensity; }
		float GetPointLightRadius() const { return pointLight_->radius; }
		void SetPointLightRadius(float radius) { pointLight_->radius = radius; }
		float GetPointLightDecay() const { return pointLight_->decay; }
		void SetPointLightDecay(float decay) { pointLight_->decay = decay; }
		bool IsPointLightEnabled() const { return pointLight_->enablePointLight != 0; }
		void SetPointLightEnabled(bool enable) { pointLight_->enablePointLight = enable; }

		// スポットライト
		const Vector4& GetSpotLightColor() const { return spotLight_->color; }
		void SetSpotLightColor(const Vector4& color) { spotLight_->color = color; }
		const Vector3& GetSpotLightPosition() const { return spotLight_->position; }
		void SetSpotLightPosition(const Vector3& position) { spotLight_->position = position; }
		const Vector3& GetSpotLightDirection() const { return spotLight_->direction; }
		void SetSpotLightDirection(const Vector3& direction) { spotLight_->direction = direction; }
		float GetSpotLightIntensity() const { return spotLight_->intensity; }
		void SetSpotLightIntensity(float intensity) { spotLight_->intensity = intensity; }
		float GetSpotLightDistance() const { return spotLight_->distance; }
		void SetSpotLightDistance(float distance) { spotLight_->distance = distance; }
		float GetSpotLightDecay() const { return spotLight_->decay; }
		void SetSpotLightDecay(float decay) { spotLight_->decay = decay; }
		float GetSpotLightCosAngle() const { return spotLight_->cosAngle; }
		void SetSpotLightCosAngle(float cosAngle) { spotLight_->cosAngle = cosAngle; }
		float GetSpotLightCosFalloffStart() const { return spotLight_->cosFalloffStart; }
		void SetSpotLightCosFalloffStart(float cosFalloffStart) { spotLight_->cosFalloffStart = cosFalloffStart; }
		bool IsSpotLightEnabled() const { return spotLight_->enableSpotLight; }
		void SetSpotLightEnabled(bool enabled) { spotLight_->enableSpotLight = enabled; }

		// シャドウマップ設定
		ShadowMapSettings& GetShadowMapSettings() { return shadowMapSettings_; }
		void SetShadowMapSettings(const ShadowMapSettings& settings) { shadowMapSettings_ = settings; }

	private:
		// コンストラクタとデストラクタ
		LightManager() = default;
		~LightManager() = default;
		// シングルトン
		LightManager(const LightManager&) = delete;
		LightManager& operator=(const LightManager&) = delete;
		LightManager(LightManager&&) = delete;
		LightManager& operator=(LightManager&&) = delete;
		///************************* GPU用の構造体 *************************///
		// 平行光源
		struct DirectionalLight {
			Vector4 color;						// ライトの色
			Vector3 direction;					// ライトの向き
			float intensity;					// 輝度
			int32_t enableDirectionalLight;		// フラグ
		};
		// ポイントライト
		struct PointLight {
			Vector4 color;						// 色
			Vector3 position;					// 位置
			float intensity;					// 輝度
			int32_t enablePointLight;

			float radius;						// ライトの届く最大距離
			float decay;						// 減衰率
			float padding[2];
		};
		// スポットライト
		struct SpotLight {
			Vector4 color;						// 色
			Vector3 position;					// 位置
			float intensity;					// 輝度
			Vector3 direction;					// 方向
			float distance;						// 最大距離
			float decay;						// 減衰率
			float cosAngle;						// 余弦
			float cosFalloffStart;				// フォールオフ開始角度（コサイン値） 追加
			int32_t enableSpotLight;			// フラグ
			float padding[2];
		};

		// シャドウ用CB
		struct ShadowMatrix {
			Matrix4x4 lightViewProjection;
		};

	private:
		///************************* メンバ変数 *************************///

		// 平行光源のリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
		DirectionalLight* directionalLight_ = nullptr;

		// ポイントライトのリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
		PointLight* pointLight_ = nullptr;

		// スポットライトのリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
		SpotLight* spotLight_ = nullptr;

		// シャドウマップ用リソース
		Microsoft::WRL::ComPtr<ID3D12Resource> shadowResource_;
		ShadowMatrix* shadow_ = nullptr;

		Object3dCommon* object3dCommon_ = nullptr;
		Camera* camera_ = nullptr;

		ShadowMapSettings shadowMapSettings_;
	};
}
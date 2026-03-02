#pragma once

// C++
#include <wrl.h>
#include <assert.h>
#include <string>
#include <vector>
#include <random>

// Engine
#include "ParticleSetting.h"

class ParticleSystem
{
public:

	// マテリアル情報
	struct MaterialInfo {
		Vector4 color = { 1.0f,1.0f,1.0f,1.0f };
		bool enableLighting = false;
		Matrix4x4 uvTransform = MakeIdentity4x4();
	};
	// ライト情報
	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};
	// 座標
	struct ParticleForGPU {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};


	// トレイル構造体
	struct TrailVertex {
		Vector3 position;
		Vector2 uv;
		Vector4 color;
	};
	struct TrailForGPU {
		Matrix4x4 WVP;
		Vector4 color;
	};



	///************************* 基本的な関数 *************************///
	ParticleSystem(const std::string& name);
	~ParticleSystem() = default;

	void InitializeResources(SrvManager* srvManager);
	void Finalize();

	void Update(float deltaTime);
	void Emit(const Vector3& position, int count = 1);
	void EmitBurst(const Vector3& position, int count);
	void PrepareInstancingData(Camera* camera);

	void InitializeTrailResources(SrvManager* srvManager);
	void FinalizeTrailResources();

	// トレイル描画データ準備
	void PrepareTrailData(Camera* camera);

	// トレイル描画用アクセサ
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTrailVertexBuffer() const { return trailVertexBuffer_; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTrailIndexBuffer() const { return trailIndexBuffer_; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTrailInstancingResource() const { return trailInstancingResource_; }
	uint32_t GetTrailSRVIndex() const { return trailSrvIndex_; }
	uint32_t GetTrailInstanceCount() const { return trailInstanceCount_; }
	size_t GetTrailVertexCount() const { return trailVertices_.size(); }
	size_t GetTrailIndexCount() const { return trailIndices_.size(); }

	///************************* アクセッサ *************************///
	ParticleSetting& GetSettings() { return settings_; }
	const ParticleSetting& GetSettings() const { return settings_; }

	// リソース設定
	void SetMesh(const std::shared_ptr<Mesh>& mesh) { mesh_ = mesh; }
	void SetTexture(const std::string& textureFilePath);

	// 状態取得
	bool IsActive() const { return isActive_; }
	void SetActive(bool active) { isActive_ = active; }
	size_t GetParticleCount() const { return particles_.size(); }
	uint32_t GetInstanceCount() const { return static_cast<uint32_t>(particles_.size()); }

	// レンダリング用アクセス
	const std::shared_ptr<Mesh>& GetMesh() const { return mesh_; }
	const std::string& GetTexture() const { return textureFilePath_; }
	uint32_t GetTextureIndexSRV() const { return textureIndexSRV_; }
	BlendMode GetBlendMode() const { return settings_.GetBlendMode(); }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetInstancingResource() const { return instancingResource_; }
	uint32_t GetSRVIndex() const { return srvIndex_; }
	const std::vector<ParticleForGPU>& GetInstancingData() const { return instancingData_; }

	// システム制御
	void SetSystemPosition(const Vector3& position) { systemPosition_ = position; }
	Vector3 GetSystemPosition() const { return systemPosition_; }
	void SetSystemRotation(const Vector3& rotation) { systemRotation_ = rotation; }
	Vector3 GetSystemRotation() const { return systemRotation_; }

	// マテリアル
	MaterialInfo GetMaterialInfo() const { return materialInfo_; }
	DirectionalLight GetDirectionalLight() const { return directionalLight_; }
private:

	///************************* 内部的な処理 *************************///
	void UpdateEmission(float deltaTime);
	void UpdateParticles(float deltaTime);
	void UpdatePhysics(ParticleData& particle, float deltaTime);
	void UpdateRotation(ParticleData& particle, float deltaTime);
	void UpdateColor(ParticleData& particle);
	void UpdateSize(ParticleData& particle);
	void UpdateVelocity(ParticleData& particle, float deltaTime);
	void UpdateForces(ParticleData& particle, float deltaTime);
	void UpdateAlpha(ParticleData& particle);
	void UpdateUV(ParticleData& particle, float deltaTime);
	void UpdateTextureSheet(ParticleData& particle, [[maybe_unused]] float deltaTime);
	void UpdateTrail(ParticleData& particle, float deltaTime);
	void RemoveDeadParticles();

	///************************* パーティクルの生成 *************************///
	ParticleData CreateParticle(const Vector3& position);
	Vector3 SampleEmissionShape();
	Vector3 GenerateRandomVelocity();
	Vector3 GenerateRandomScale();
	Vector3 GenerateRandomRotation();
	Vector3 GenerateRandomRotationVelocity();
	float GenerateRandomLifeTime();
	float GenerateRandomMass();
	Vector4 GenerateRandomColor();

	///************************* 色 *************************///
	void UpdateFadeColor(ParticleData& particle);
	void UpdateFireColor(ParticleData& particle);
	void UpdateRainbowColor(ParticleData& particle);
	void UpdateFlashColor(ParticleData& particle);
	void UpdateGradientColor(ParticleData& particle);
	void UpdateElectricColor(ParticleData& particle);
	Vector4 HSVtoRGB(float h, float s, float v, float a);

	///************************* 物理 *************************///
	void ApplyGravity(ParticleData& particle, float deltaTime);
	void ApplyDrag(ParticleData& particle, float deltaTime);
	void ApplyTurbulence(ParticleData& particle, float deltaTime);
	void ApplyVortex(ParticleData& particle, float deltaTime);

	float PerlinNoise(float x, float y, float z, float time);

	///************************* 回転 *************************///
	void InitializeRotation(ParticleData& particle);
	void UpdateRotationVelocity(ParticleData& particle, float deltaTime);
	Vector3 ApplyRotationAcceleration(const Vector3& velocity, const Vector3& acceleration, float deltaTime);
	Vector3 ApplyRotationDamping(const Vector3& velocity, float damping, float deltaTime);

	///************************* マテリアル *************************///
	void UpdateMaterialInfo();
	void UpdateSystemUVTransform();


	///************************* コンボ情報取得 *************************///
	float GetRandomFloat(float min, float max);
	Vector3 GetRandomVector3(const Vector3& min, const Vector3& max);
	Vector3 GetRandomDirection();
	Vector4 LerpColor(const Vector4& start, const Vector4& end, float t);
	float EaseInOut(float t);
	float SmoothStep(float t);

private:

	///************************* メンバ変数 *************************///
	std::string name_;
	ParticleSetting settings_;
	std::vector<ParticleData> particles_;

	// システム位置・回転
	Vector3 systemPosition_;
	Vector3 systemRotation_;
	Vector3 systemVelocity_;
	Vector3 previousSystemPosition_;

	// リソース
	std::shared_ptr<Mesh> mesh_;
	std::string textureFilePath_;
	uint32_t textureIndexSRV_;

	// DirectX12リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	uint32_t srvIndex_;
	ParticleForGPU* instancingDataForGPU_;
	std::vector<ParticleForGPU> instancingData_;

	// エミッション制御
	float emissionTimer_;
	float systemTime_;
	bool isActive_;
	bool hasStarted_;

	// バースト制御
	float burstTimer_;
	int burstCount_;

	// 内部制御
	std::mt19937 randomEngine_;
	std::random_device randomDevice_;

	static const uint32_t kMaxInstances_ = 10000;
	uint32_t instanceCount_ = 0;

	// マテリアル情報
	MaterialInfo materialInfo_;
	DirectionalLight directionalLight_;


	Vector2 uvOffset_ = { 0.0f, 0.0f };           // アニメーション用オフセット
	Vector2 currentUVScale_ = { 1.0f, 1.0f };     // 現在のスケール
	float uvRotation_ = 0.0f;                     // アニメーション用回転角
	float lastDeltaTime_ = 0.0f;                  // デルタタイム保存用



	Microsoft::WRL::ComPtr<ID3D12Resource> trailVertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> trailIndexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> trailInstancingResource_;
	uint32_t trailSrvIndex_;
	std::vector<TrailVertex> trailVertices_;
	std::vector<uint32_t> trailIndices_;
	std::vector<TrailForGPU> trailInstancingData_;
	TrailForGPU* trailInstancingDataForGPU_;

	static const uint32_t kMaxTrailInstances_ = 5000;
	uint32_t trailInstanceCount_ = 0;
};

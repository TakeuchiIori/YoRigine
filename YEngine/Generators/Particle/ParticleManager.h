#pragma once

#include "ParticleRenderer.h"
#include "ParticleSystem.h"

/// <summary>
/// CPUパーティクルの管理クラス
/// </summary>
namespace YoRigine {
	class ParticleManager {
	public:
		///************************* 基本関数 *************************///

		// パフォーマンス情報
		struct PerformanceInfo {
			int totalParticles = 0;
			int activeGroups = 0;
			float updateTime = 0.0f;
			float renderTime = 0.0f;
		};
		static ParticleManager* GetInstance();
		ParticleManager() = default;
		~ParticleManager() = default;
		void Initialize(SrvManager* srvManager);
		void Update(float deltaTime);
		void Draw();
		void Finalize();
		void SetCamera(Camera* camera);
		///************************* パーティクルシステム管理 *************************///
		void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);
		void SetPrimitiveMesh(const std::string& groupName, const std::shared_ptr<Mesh>& mesh);
		void Emit(const std::string& name, const Vector3& position, uint32_t count);
		void EmitBurst(const std::string& groupName, const Vector3& position, int count);

		///************************* パラメータのセット *************************///
		void SetGravity(const std::string& name, const Vector3& gravity);
		void SetColor(const std::string& name, const Vector4& startColor, const Vector4& endColor);
		void SetEmissionRate(const std::string& name, float rate);
		void SetSpeed(const std::string& name, float speed);
		void SetLifeTime(const std::string& name, const Vector2& lifeTimeRange);
		void SetBlendMode(const std::string& name, BlendMode blendMode);


		///************************* アクセッサ *************************///
		ParticleSystem* GetSystem(const std::string& name);
		const PerformanceInfo& GetPerformanceInfo() const { return performanceInfo_; }
		std::vector<std::string> GetAllSystemNames() const;
	private:
		// シングルトン
		ParticleManager(const ParticleManager&) = delete;
		ParticleManager& operator=(const ParticleManager&) = delete;
		///************************* メンバ変数 *************************///

		static std::unique_ptr<ParticleManager> instance_;
		static std::once_flag initFlag_;
		std::unordered_map<std::string, std::unique_ptr<ParticleSystem>> systems_;
		std::unique_ptr<ParticleRenderer> renderer_;
		SrvManager* srvManager_;
		bool initialized_;
		mutable PerformanceInfo performanceInfo_;
	};
}
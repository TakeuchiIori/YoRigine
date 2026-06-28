#pragma once
#include <vector>
#include <memory>
#include <string>

#include "YParticleStructs.h"
#include "Modules/ParticleAttribute.h"
#include "Modules/IParticleModule.h"
#include "Mesh/Mesh.h"
#include "Mesh/MeshPrimitive.h"
#include "Loaders/Json/EnumUtils.h"
#include "FileOperations/FileBrowser.h"



class YParticleSystem
{
public:
	///************************* インスタンス*************************///
	YParticleSystem(const std::string& name, uint32_t maxParticles = 1000);
	~YParticleSystem() = default;

	///************************* モジュールの追加と削除 *************************///
	void AddSpawnModule(std::shared_ptr<ISpawnModule> module);
	void AddUpdateModule(std::shared_ptr<IUpdateModule> module);

	void DeleteSpawnModule(const std::shared_ptr<ISpawnModule>& module);
	void DeleteUpdateModule(const std::shared_ptr<IUpdateModule>& module);

	///************************* メインとなる処理 *************************///
	void Update(float deltaTime);
	void Emit(const Vector3& position, int count = 1);


public:
	///************************* アクセッサ *************************///
	std::shared_ptr<IParticleModule> GetModule(const std::string& moduleName) const;
	const std::string& GetName() const { return name_; }
	void SetName(const std::string& name) { name_ = name; }

	// 描画に必要な情報を公開する
	const std::vector<ParticleAttribute>& GetAttributes() const { return attributes_; }
	uint32_t GetMaxParticles() const { return maxParticles_; }

	// テクスチャ関連
	void SetTexture(const std::string& textureFilePath);
	uint32_t GetTextureIndex() const { return textureIndexSRV_; }
	std::string GetTextureFilePath() const { return textureFilePath_; }

	// メッシュ（プリミティブ）関連
	void SetMeshPrimitive(const std::shared_ptr<Mesh>& mesh) { mesh_ = mesh; }
	std::shared_ptr<Mesh> GetMesh() const { return mesh_; }
	int GetCurrentMeshType() const { return currentMeshType_; }
	const float* GetMeshParams() const { return meshParams_; }

	/// <summary>
	/// メッシュタイプとパラメータを設定
	/// </summary>
	/// <param name="meshType">メッシュタイプ (0:Plane, 1:Box, 2:Ring, 3:Cylinder)</param>
	/// <param name="params">パラメータ配列 (JSONから読み込んだ配列)</param>
	void SetMeshType(int meshType, const nlohmann::json& params);

	/// <summary>
	/// メッシュタイプとパラメータを直接設定
	/// </summary>
	void SetMeshTypeAndParams(int meshType, const float* params);


	// 親のトランスフォーム（エミッター等の座標）をセット
	void SetParentMatrix(const Matrix4x4& parentMatrix) { parentMatrix_ = parentMatrix; }
	void SetRelative(bool isRelative) { isRelative_ = isRelative; }

	bool IsRelative() const { return isRelative_; }
	const Matrix4x4& GetParentMatrix() const { return parentMatrix_; }

	// ビルボード設定
	void SetBillboardType(BillboardType type) { billboardType_ = type; }
	BillboardType GetBillboardType() const { return billboardType_; }
	uint32_t GetBillboardTypeAsUInt() const { return static_cast<uint32_t>(billboardType_); }

	// ライト設定
	void SetLightSetting(const ParticleLightSetting& setting) { lightSetting_ = setting; }
	const ParticleLightSetting& GetLightSetting() const { return lightSetting_; }

	// 後方互換：いずれかのライトが有効なら true
	bool IsEnableLight() const {
		return lightSetting_.enableDirectionalLight
			|| lightSetting_.enablePointLight
			|| lightSetting_.enableSpotLight;
	}

	// モジュール群の取得
	const std::vector<std::shared_ptr<ISpawnModule>>& GetSpawnModules() const {
		return spawnModules_;
	}
	const std::vector<std::shared_ptr<IUpdateModule>>& GetUpdateModules() const {
		return updateModules_;
	}

	// ブレンドモードの設定
	void SetBlendMode(BlendMode mode) { blendMode_ = mode; }
	BlendMode GetBlendMode() const { return blendMode_; }

#ifdef USE_IMGUI
	///************************* エディタ関数 *************************///
	void ShowEditor();
#endif

private:
	///************************* 内部処理 *************************///
	int FindEmptyIndex() const;
	void RefreshMesh();

private:
	std::string name_;
	uint32_t maxParticles_;


	// テクスチャ関連
	uint32_t textureIndexSRV_ = 0;
	std::string textureFilePath_;
	const std::string textureDirectory_ = "Resources/Effects/";
#ifdef USE_IMGUI
	YoRigine::FileBrowser textureBrowser_;
#endif
	bool showTextureBrowser_ = false;

	// 全パーティクルの共通データ
	std::vector<ParticleAttribute> attributes_;

	// モジュール群
	std::vector<std::shared_ptr<ISpawnModule>> spawnModules_;
	std::vector<std::shared_ptr<IUpdateModule>> updateModules_;

	// メッシュ（プリミティブ）
	std::shared_ptr<Mesh> mesh_ = nullptr;
	int currentMeshType_ = 0;
	float meshParams_[4] = { 1.0f, 1.0f, 1.0f, 16.0f };


	ParticleLightSetting lightSetting_;   // ライト設定

	bool isRelative_ = false;       // trueなら親と一緒に動く
	Matrix4x4 parentMatrix_;       // 親（エミッター）の現在の行列
	BillboardType billboardType_ = BillboardType::Full;   // ビルボードタイプ

	BlendMode blendMode_ = BlendMode::kBlendModeNormal; // ブレンドモード
};
#pragma once
//Engine
#include <DirectXCommon.h>
#include <GPUParticle/GPUParticle.h>
#include <Systems/Camera/Camera.h>
#include "GpuParticleParams.h"
// Math
#include <Matrix4x4.h>
#include <Vector3.h>


#include <DirectXMath.h>
#include <Model.h>


enum class EmitterShape : uint32_t
{
	Sphere = 0,
	Box = 1,
	Triangle = 2,
	Cone = 3,
	Mesh = 4
};

enum class MeshEmitMode : uint32_t
{
	Surface = 0,   // メッシュの表面
	Volume = 1,    // メッシュの内部
	Edge = 2       // メッシュのエッジ
};

// 1粒子として描画するメッシュ形状（板ポリ以外も選べる）
enum class ParticleMeshShape : uint32_t
{
	Plane = 0,     // 板ポリ（既定・ビルボード向き）
	Box = 1,       // 立方体
	Ring = 2,      // リング
	Cylinder = 3,  // 円柱
	Sphere = 4,    // 球
	Cone = 5,      // 円錐
	Fan = 6,       // 扇形
};
static const uint32_t kParticleMeshShapeCount = 7;

// 粒子メッシュ形状ごとの生成パラメータ（使うフィールドは形状で異なる）。
// 実サイズは粒子ごとの scale で拡縮されるため、ここは「基準の形状」を決める。
struct ParticleMeshParams
{
	float    width = 1.0f;         // Plane/Box: 幅
	float    height = 1.0f;        // Plane/Box: 高さ, Cylinder/Cone: 高さ
	float    depth = 1.0f;         // Box: 奥行き
	float    outerRadius = 0.5f;   // Ring/Cylinder: 外周半径
	float    innerRadius = 0.25f;  // Ring/Cylinder: 内周半径
	float    radius = 0.5f;        // Sphere/Cone/Fan: 半径
	float    angleDegree = 90.0f;  // Fan: 扇の角度
	uint32_t divide = 16;          // Ring/Cylinder/Cone/Fan: 分割数
	uint32_t subdivisions = 2;     // Sphere: 細分化レベル
};


struct TrailParams;
/// <summary>
/// エミッタークラス
/// </summary>
class GPUEmitter
{
public:
	///************************* GPUバッファ用の構造体 *************************///
	__declspec(align(16))
		struct EmitterCommonData {
		uint32_t emitterShape;
		float padding[3];
	};
	static_assert(sizeof(EmitterCommonData) == 16, "EmitterCommonData must be 16 bytes");

	__declspec(align(16))
		struct EmitterSphereData {
		Vector3 translate;
		float radius;
		float count;
		float emitInterval;
		float intervalTime;
		uint32_t isEmit;
	};
	static_assert(sizeof(EmitterSphereData) == 32, "EmitterSphereData must be 32 bytes");

	__declspec(align(16))
		struct EmitterBoxData {
		Vector3 translate;
		float padding[1];
		Vector3 size;
		float count;
		float emitInterval;
		float intervalTime;
		uint32_t isEmit;
		float padding2[1];
	};
	static_assert(sizeof(EmitterBoxData) == 48, "EmitterBoxData must be 48 bytes");

	__declspec(align(16))
		struct EmitterTriangleData {
		Vector3 v1;
		float pad0;
		Vector3 v2;
		float pad1;
		Vector3 v3;
		float pad2;
		Vector3 translate;
		float pad3;
		float count;
		float emitInterval;
		float intervalTime;
		uint32_t isEmit;
	};
	static_assert(sizeof(EmitterTriangleData) == 80, "EmitterTriangleData must be 80 bytes");

	__declspec(align(16))
		struct EmitterConeData {
		Vector3 translate;    float pad0;
		Vector3 direction;    float radius;
		float height;
		float count;
		float emitInterval;
		float intervalTime;
		uint32_t isEmit;
		float padding[3];
	};
	static_assert(sizeof(EmitterConeData) == 64, "EmitterConeData must be 64 bytes");

	__declspec(align(16))
		struct PerFrameData {
		float time;
		float deltaTime;
		float padding[2];
	};

	// メッシュエミッター
	__declspec(align(16))
		struct EmitterMeshData {
		Vector3 translate;
		float pad0;
		Vector3 scale;
		float pad1;
		Vector4 rotation;  // クォータニオン (x, y, z, w)
		float count;
		float emitInterval;
		float intervalTime;
		uint32_t isEmit;
		uint32_t emitMode;
		uint32_t triangleCount;
		float padding[2];
	};
	static_assert(sizeof(EmitterMeshData) == 80, "EmitterMeshData must be 80 bytes");

	// メッシュ三角形データ
	struct MeshTriangle {
		Vector3 v0;
		Vector3 v1;
		Vector3 v2;
		Vector3 normal;
		float area;
		uint32_t activeEdges;
	};
	static_assert(sizeof(MeshTriangle) == 56, "MeshTriangle must be 64 bytes");


	static_assert(sizeof(PerFrameData) == 16, "PerFrameData must be 16 bytes");


	__declspec(align(16))
		struct ChildParticleParamsForGPU
	{
		uint32_t isTrail;
		uint32_t inheritScale;
		float minDistance;
		float lifeTime;
		int emissionCount;


		float startScale;
		float endScale;
		float pad0;
	};

	__declspec(align(16))
		struct ParticleParameters
	{
		// 生存時間 ---------------------------------------------------
		float lifeTime;
		float lifeTimeVariance;
		float pad0[2];

		// スケール ---------------------------------------------------
		Vector3 startScale;
		float   pad1;

		Vector3 startScaleVariance;
		float   pad2;

		Vector3 endScale;
		float   pad3;

		Vector3 endScaleVariance;
		float   pad4;

		// 回転 -------------------------------------------------------
		float rotation;
		float rotationVariance;
		float rotationSpeed;
		float rotationSpeedVariance;

		// 速度 -------------------------------------------------------
		Vector3 velocity;
		float   pad5;

		Vector3 velocityVariance;
		float   pad6;

		// 色 ---------------------------------------------------------
		Vector4 startColor;
		Vector4 startColorVariance;
		Vector4 endColor;
		Vector4 endColorVariance;

		// 物理 ---------------------------------------------------------
		float gravity;

		// ビルボード設定 ---------------------------------------------
		uint32_t isBillboard;
		float    pad7[2];

		ChildParticleParamsForGPU childParams;
	};



public:
	///************************* 基本関数 *************************///
	void Initialize(Camera* camera, std::string& texturePath);
	void Update(float dt);
	void Draw();
	void Reset();

	// エミッター形状設定
	void SetEmitterShape(EmitterShape shape);
	void SetSphereParams(const Vector3& translate, float radius, float count, float emitInterval);
	void SetBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval);
	void SetTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& translate, float count, float emitInterval);
	void SetConeParams(const Vector3& translate, const Vector3& direction, float radius, float height, float count, float emitInterval);
	void SetMeshParams(Model* model, const Vector3& translate, const Vector3& scale,
		const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode = MeshEmitMode::Surface);


	// エミッターの更新用
	void UpdateSphereParams(const Vector3& translate, float radius, float count, float emitInterval);
	void UpdateBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval);
	void UpdateTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& translate, float count, float emitInterval);
	void UpdateConeParams(const Vector3& translate, const Vector3& direction, float radius, float height, float count, float emitInterval);
	void UpdateMeshParams(Model* model, const Vector3& translate, const Vector3& scale,
		const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode = MeshEmitMode::Surface);

	// パーティクルパラメータ設定
	void SetParticleParameters(const ParticleParams& params);

	// 1粒子として描画するメッシュ形状を差し替える（形状＋生成パラメータ）
	void SetParticleMesh(ParticleMeshShape shape, const ParticleMeshParams& params);
	ParticleMeshShape GetParticleMeshShape() const { return particleMeshShape_; }

	///************************* 外部から呼ぶ *************************///

	// 指定位置にパーティクルを放出
	void EmitAtPosition(const Vector3& position, float count);

	// 現在の形状の放出位置(translate)だけをワールド座標へ上書きする（継続発生の追従用）
	void SetEmitWorldPosition(const Vector3& worldPos);

	// 継続発生（interval駆動）の ON/OFF。OFFでも Update() による粒子シミュレーションは継続する
	void SetContinuousEmit(bool enable) { continuousEmit_ = enable; }
	// 次フレームに1回だけ発生させる（ワンショット。continuousEmit_ と独立）
	void RequestBurst() { ++burstRequest_; }

private:
	///************************* 内部処理 *************************///
	void CreateEmitterResources();
	void CreateParticleParametersResource();
	void CreatePerFrameResource();
	void CreateMeshTriangleBuffer();
	void UpdateMeshTriangleData(Model* model);
	void Dispatch();

	void UpdateEmitters();
	void UpdateEmitterTrail();

public:
	///************************* アクセッサ *************************///
	GPUParticle* GetGPUParticle() const { return gpuParticle_.get(); }
	EmitterShape GetCurrentShape() const { return currentShape_; }
	MeshEmitMode GetCurrentMeshMode() const { return currentMeshMode_; }
	Vector3 GetEmitterPosition() const;

	void SetTrailParams(const EmitterTrailParams& params) { trail_ = params; };
	void SetCamera(Camera* camera);
private:
	///************************* メンバ変数 *************************///
	Camera* camera_ = nullptr;
	std::unique_ptr<GPUParticle> gpuParticle_;
	const uint32_t kMaxEmitters_ = 1;
	const uint32_t threadsPerGroup_ = 1024;
	const uint32_t kMaxTriangles_ = 200000;

	EmitterShape currentShape_ = EmitterShape::Sphere;
	MeshEmitMode currentMeshMode_ = MeshEmitMode::Surface;
	ParticleMeshShape particleMeshShape_ = ParticleMeshShape::Plane;  // 1粒子の描画メッシュ形状

	// 発生制御（emission と simulation の分離）
	bool continuousEmit_ = false;   // interval駆動の継続発生（マネージャが isPlaying に応じて毎フレーム設定）
	int  burstRequest_ = 0;         // 未処理のワンショット発生要求数

	// 各エミッター用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterCommonResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterSphereResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterBoxResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterTriangleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterConeResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterMeshResource_;  // 新規追加
	Microsoft::WRL::ComPtr<ID3D12Resource> meshTriangleBuffer_;   // 新規追加
	Microsoft::WRL::ComPtr<ID3D12Resource> perframeResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleParametersResource_;

	// データポインタ
	EmitterCommonData* emitterCommonData_ = nullptr;
	EmitterSphereData* emitterSphereData_ = nullptr;
	EmitterBoxData* emitterBoxData_ = nullptr;
	EmitterTriangleData* emitterTriangleData_ = nullptr;
	EmitterConeData* emitterConeData_ = nullptr;
	EmitterMeshData* emitterMeshData_ = nullptr;  // 新規追加
	MeshTriangle* meshTriangleData_ = nullptr;    // 新規追加
	PerFrameData* perframeData_ = nullptr;
	ParticleParameters* particleParameters_ = nullptr;

	// メッシュ関連
	std::vector<MeshTriangle> meshTriangles_;  // CPUサイドのキャッシュ
	uint32_t meshTriangleBufferSrvIndex_ = 0;  // SRVインデックス
	Model* currentMeshModel_ = nullptr;
	float timeScalelastEmit_ = 0.0f;

	EmitterTrailParams trail_;
	Vector3 lastEmitWorldPos_{};
	bool hasLastEmitWorldPos_ = false;
	Vector3 trailLastPos_{};   // 実行状態
	bool    trailHasLast_ = false;
};
#pragma once

// Engine
#include "Object3D./Object3d.h"
#include "Systems/Input./Input.h"
#include "WorldTransform./WorldTransform.h"
#include "Collision/Sphere/SphereCollider.h"
#include "Collision/AABB/AABBCollider.h"
#include "Collision/OBB/OBBCollider.h"
#include "Collision/Core/ColliderFactory.h"
#include "Loaders/Json/JsonManager.h"
#include "Systems/Camera/Camera.h"
#include <Systems/GameTime/GameTIme.h>
#include "Collision/Core/CollisionDirection.h"
// C++
#include <memory>

// Math
#include "MathFunc.h"
#include "Vector3.h" 
#include "Particle/ParticleEmitter.h"

class EnemyManager;
class Player;
class Enemy
{
public:

	Enemy();
	~Enemy();
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Camera* camera,const Vector3& pos);

	void InitCollision();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();
	void DrawCollision();

	/// <summary>
	/// ImGui
	/// </summary>
	void ShowCoordinatesImGui();


	// 衝突イベント（共通で受け取る）
	void OnEnterCollision(BaseCollider* self, BaseCollider* other);
	void OnCollision(BaseCollider* self, BaseCollider* other);
	void OnExitCollision(BaseCollider* self, BaseCollider* other);
	void OnDirectionCollision(BaseCollider* self, BaseCollider* other, HitDirection dir);

	/// <summary>
	/// 
	/// </summary>
	void EnemyAllHitStop();


private:

	/// <summary>
	/// 移動処理
	/// </summary>
	void Move();

	/// <summary>
	/// カメラシェイク
	/// </summary>
	void CameraShake();


	void InitJson();

	Vector3 GetWorldPosition();

public: // アクセッサ
	/// <summary>
	/// ワールド変換データを取得
	/// </summary>
	/// <returns>"ワールド変換データ</returns>
	const WorldTransform& GetWorldTransform() { return worldTransform_; }
	const Vector3& GetPosition() const { return worldTransform_.translation_; }
	const Vector3& GetRotation() const { return worldTransform_.rotation_; }
	void SetPosition(const Vector3& pos) { worldTransform_.translation_ = pos; }
	void SetPlayer(Player* player) { player_ = player; }; // プレイヤーをセットする関数
	void SetEnemyManager(EnemyManager* manager) { enemyManager_ = manager; }

	/// <summary>
	/// シリアルナンバーの取得
	/// </summary>
	uint32_t GetSerialNumber() const { return serialNumber_; }
	bool IsActive() const { return isActive_; }


	void SetHP(int hp) { hp_ = hp; }
	int GetHP() { return hp_; }

private:
	EnemyManager* enemyManager_;
	Input* input_ = nullptr;
	Camera* camera_ = nullptr;
	Player* player_ = nullptr; 
	std::unique_ptr <JsonManager> jsonManager_;
	std::unique_ptr <JsonManager> jsonCollider_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;
	std::unique_ptr<Object3d> shadow_;
	std::unique_ptr<Object3d> base_ = nullptr;

	//std::shared_ptr<OBBCollider> obbCollider_;
	std::shared_ptr<AABBCollider> aabbCollider_;
	std::shared_ptr<SphereCollider> sphereCollider_;


	WorldTransform worldTransform_;
	WorldTransform WS_;

	//float radius_ = 4.0f;
	float s_;
	float speed_ = 0.15f;
	bool isColliding_ = false;
	Vector3 moveSpeed_;
	bool isDrawEnabled_ = true;
	bool isActive_ = true;
	bool isShake_ = false;

	uint32_t serialNumber_ = 0;
	static uint32_t nextSerialNumber_;


	int hp_ = 100;
	bool isAlive_ = true;
	bool isHit_ = false;


	std::string timeID_;
	GameTime* gameTime_ = nullptr;
	float deltaTime_;

	float baseSpeed_ = 0.25f;
	float rotationSpeed_ = 0.1f;     // 回転速度（ラジアン/秒）
	float radius_ = 4.0f;            // 敵の衝突半径
	const float weaponRadius_ = 2.0f; // プレイヤーの武器の半径
};


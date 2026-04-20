#pragma once
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Loaders/Json/JsonManager.h"
#include "Collision/Core/CollisionDirection.h"
#include "Collision/OBB/OBBCollider.h"
#include <Particle/ParticleEmitter.h>
#include "Object3d/BaseObject.h"

class Player;

// ============================================================
// プレイヤー盾クラス
// プレイヤーの腕のボーンに追従し、ガードやパリィ判定のコライダーを提供する
// ============================================================
class PlayerShield : public BaseObject
{
public:
	// ============================================================
	// 初期化と更新処理
	// ============================================================
	~PlayerShield();
	void Initialize(Camera* camera) override;

	void Update() override;

	void Draw() override;
	void DrawShadow();
	void DrawCollision() override;

	// ============================================================
	// アクセッサ・状態操作
	// ============================================================
	void SetPlayer(Player* player) { player_ = player; }
	void SetObject(Object3d* obj3d) { obj3d_ = obj3d; }
	bool IsJointValid() const { return isValidJoint_; }
	WorldTransform& GetWorldTransform() { return wt_; }

	void SetEnableCollider(bool enable) {
		obbCollider_->SetCollisionEnabled(enable);
	}

	// ============================================================
	// 当たり判定コールバック
	// ============================================================
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir);
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void InitCollision() override;
	void InitJson() override;

	void FindHandJointIndex();
	void SetPlayerWeaponPosition();

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム連携・参照
	// ------------------------------------------------------------
	Camera* camera_ = nullptr;            // 描画に使用するカメラ
	Player* player_ = nullptr;            // 盾を装備しているプレイヤー本体
	Object3d* obj3d_ = nullptr;           // プレイヤーの3Dモデル（ジョイント探索用）

	// ------------------------------------------------------------
	// パーティクル
	// ------------------------------------------------------------
	std::unique_ptr<ParticleEmitter> testEmitter_; // ガード成功時などのエフェクト

	// ------------------------------------------------------------
	// ジョイントアタッチ関連
	// ------------------------------------------------------------
	std::string handJointName_ = "mixamorig:LeftHand";  // アタッチ先の手ジョイント名
	int handleIndex_ = 0;                               // ジョイントの配列インデックス
	bool isValidJoint_ = false;                         // 正しいジョイントが見つかったか

	Vector3 offsetPos_{};                               // 手からの位置オフセット
	Vector3 offsetRot_{};                               // 手からの回転オフセット
	Vector3 offsetScale_{ 1.0f,1.0f,1.0f };             // 盾のスケール
};
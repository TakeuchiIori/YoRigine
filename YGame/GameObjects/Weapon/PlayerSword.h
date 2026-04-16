#pragma once
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Loaders/Json/JsonManager.h"
#include "Collision/Core/CollisionDirection.h"
#include "Collision/OBB/OBBCollider.h"
#include <Particle/ParticleEmitter.h>
#include "Object3d/BaseObject.h"

#include "Particle/YEmitterGroup.h"
#include <Vfx/VfxMesh/TrailMeshEmitter.h>
class Player;
/// <summary>
/// プレイヤーの剣クラス
/// </summary>
class PlayerSword : public BaseObject
{
public:
	///************************* 基本関数 *************************///

	void Initialize(Camera* camera) override;
	void Update()override;
	void Draw()override;
	void DrawShadow();
	void DrawCollision()override;
	void DrawVfx();

	///************************* 当たり判定 *************************///
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir);
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

	///************************* VFXの操作 *************************///
	void PlayTrail() { if (trailEmitter_) trailEmitter_->Play(); }
	void StopTrail() { if (trailEmitter_) trailEmitter_->Stop(); }

public:
	///************************* アクセッサ *************************///
	/// ジョイントが有効かどうか
	bool IsJointValid() const { return isValidJoint_; }
private:
	///************************* 内部処理 *************************///

	void InitCollision()override;
	void InitJson()override;

	// 手ジョイントのインデックスを探す
	void FindHandJointIndex();
	// 武器の位置をプレイヤーに合わせる
	void SetPlayerWeaponPosition();
	// コライダー用のワールド変換を更新
	void UpdateColliderWorldTransform();
	Vector3 GetHandPosition();
	// 行列から平行移動成分を抽出
	Vector3 ExtractTranslation(const Matrix4x4& matrix);


public:
	///************************* アクセッサ *************************///

	OBBCollider* GetOBBCollider() { return obbCollider_.get(); }
	void SetEnableCollider(bool enable) {
		obbCollider_->SetCollisionEnabled(enable);
	}
	Vector3 GetWowldPosition();

	/// プレイヤーのセット
	void SetPlayer(Player* player) { player_ = player; }
	void SetObject(Object3d* obj3d) { obj3d_ = obj3d; }

	// トレイルエフェクトの描画設定
	void SetisDrawTrail(bool isDraw) { isDrawTrail_ = isDraw; }

private:
	///************************* ポインタ *************************///
	Camera* camera_ = nullptr;
	Object3d* obj3d_ = nullptr;
	Player* player_ = nullptr;

	WorldTransform colliderWT_;  // コライダー用のワールド変換
	std::unique_ptr<ParticleEmitter> hitParticleEmitter_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;
	std::unique_ptr<ParticleEmitter> testEmitter_;



	///************************* ジョイント関連 *************************///
	std::string handJointName_ = "mixamorig:RightHand";  // 手ジョイント名
	int handleIndex_ = 0;                                // 手ジョイントのインデックス
	bool isValidJoint_ = false;                          // ジョイントが有効かどうか

	Vector3 offsetPos_{};
	Vector3 offsetRot_{};
	Vector3 offsetScale_{ 1.0f,1.0f,1.0f };
	Matrix4x4 finalMatrix_;

	///************************* VFX *************************///
	std::unique_ptr<YoRigine::TrailMeshEmitter> trailEmitter_;
	bool isDrawTrail_ = false;
	Vector3 localRoot = { 0.f, 0.f, 0.f };
	Vector3 localTip = { 0.f, 1.0f, 0.f };
};
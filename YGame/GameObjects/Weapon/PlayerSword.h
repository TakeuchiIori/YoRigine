#pragma once
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Loaders/Json/JsonManager.h"
#include "Collision/Core/CollisionDirection.h"
#include "Collision/OBB/OBBCollider.h"
#include "Object3d/BaseObject.h"
#include "Particle/EffectHandle.h"      // YParticle 統合ヘルパー（Group/System を名前で解決）
#include <Vfx/VfxMesh/Runtime/TrailMeshEmitter.h>
class Player;

// ============================================================
// プレイヤー武器（剣）クラス
// プレイヤーの腕のボーンに追従し、攻撃判定用コライダーや剣の軌跡エフェクトを管理する
// ============================================================
class PlayerSword : public BaseObject
{
public:
	// ============================================================
	// 初期化と更新処理
	// ============================================================

	void Initialize(Camera* camera) override;
	void Update() override;
	void Draw() override;
	void DrawShadow();
	void DrawCollision() override;
	void DrawVfx();

	// ============================================================
	// 当たり判定コールバック
	// ============================================================
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir);
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

	// ============================================================
	// VFXの操作
	// ============================================================
	void LoadVfxAssets(const std::string vfxAssetPath);
	void PlayTrail() { if (trailEmitter_) trailEmitter_->Play(); }
	void StopTrail() { if (trailEmitter_) trailEmitter_->Stop(); }

	// ============================================================
	// アクセッサ・状態操作
	// ============================================================
	bool IsJointValid() const { return isValidJoint_; }

	OBBCollider* GetOBBCollider() { return obbCollider_.get(); }
	void SetEnableCollider(bool enable) {
		obbCollider_->SetCollisionEnabled(enable);
	}
	Vector3 GetWowldPosition();

	void SetPlayer(Player* player) { player_ = player; }
	void SetObject(Object3d* obj3d) { obj3d_ = obj3d; }

	void SetisDrawTrail(bool isDraw) { isDrawTrail_ = isDraw; }

	// スタイルに応じた見た目の切替。魔法スタイルなら杖(Staff)、剣スタイルなら剣を描画する。
	// 当たり判定(剣の近接OBB)はどちらの見た目でも wt_ に紐づいたまま。魔法時は
	// PlayerCombat が近接コライダーを有効化しないため、実質そのまま無害。
	void SetMagicVisual(bool magic) { isMagicVisual_ = magic; }
	bool IsMagicVisual() const { return isMagicVisual_; }

private:
	// ============================================================
	// 内部処理
	// ============================================================

	void InitCollision() override;
	void InitJson() override;

	void FindHandJointIndex();
	void SetPlayerWeaponPosition();
	// 手ジョイントを基準に、与えたオフセットからワールド行列を組む共通処理。
	// 剣(wt_)・杖(staffWT_)の両方をこれで配置する。
	Matrix4x4 ComposeWeaponMatrixFromHand(const Vector3& pos, const Vector3& rot, const Vector3& scale) const;
	void UpdateColliderWorldTransform();
	Vector3 GetHandPosition();
	Vector3 ExtractTranslation(const Matrix4x4& matrix);

private:
	// ============================================================
	// メンバ変数
	// ============================================================

	// ------------------------------------------------------------
	// システム連携・参照
	// ------------------------------------------------------------
	Camera* camera_ = nullptr;            // 描画に使用するカメラ
	Object3d* obj3d_ = nullptr;           // プレイヤーの3Dモデル（ジョイント探索用）
	Player* player_ = nullptr;            // 剣を装備しているプレイヤー本体

	// ------------------------------------------------------------
	// コライダー・パーティクル
	// ------------------------------------------------------------
	WorldTransform colliderWT_;                                 // コライダー用の独立したワールド変換
	// エフェクトハンドル（YParticle 統合）
	EffectHandle hitEffect_;     // ヒット時エフェクト
	EffectHandle trailEffect_;   // 通常スラッシュエフェクト

	// ------------------------------------------------------------
	// ジョイントアタッチ関連
	// ------------------------------------------------------------
	std::string handJointName_ = "mixamorig:RightHand";  // アタッチ先の手ジョイント名
	int handleIndex_ = 0;                                // ジョイントの配列インデックス
	bool isValidJoint_ = false;                          // 正しいジョイントが見つかったか

	Vector3 offsetPos_{};                                // 手からの位置オフセット
	Vector3 offsetRot_{};                                // 手からの回転オフセット
	Vector3 offsetScale_{ 1.0f,1.0f,1.0f };              // 剣のスケール
	Matrix4x4 finalMatrix_;                              // 計算済みの最終的なワールド行列

	// ------------------------------------------------------------
	// 杖（魔法スタイル時の見た目）関連
	//   Blender で中央原点エクスポートされているため、手のグリップに合わせる
	//   オフセットは剣とは別に持たせて JSON で調整できるようにする。
	// ------------------------------------------------------------
	std::unique_ptr<Object3d> staffObj_;                 // 杖モデル（Staff.obj）
	WorldTransform staffWT_;                             // 杖用の独立したワールド変換
	Vector3 staffOffsetPos_{};                           // 手からの位置オフセット（中央原点前提で調整）
	Vector3 staffOffsetRot_{};                           // 手からの回転オフセット
	Vector3 staffOffsetScale_{ 1.0f,1.0f,1.0f };         // 杖のスケール
	bool isMagicVisual_ = false;                         // true の間は剣ではなく杖を描画する

	// ------------------------------------------------------------
	// トレイル（剣の軌跡）関連
	// ------------------------------------------------------------
	std::unique_ptr<YoRigine::TrailMeshEmitter> trailEmitter_; // トレイルメッシュエミッタ
	bool isDrawTrail_ = false;                                 // トレイルを描画するかどうか
	Vector3 localRoot = { 0.f, 0.f, 0.f };                     // 剣の根元のローカル座標
	Vector3 localTip = { 0.f, 1.0f, 0.f };                     // 剣の先端のローカル座標
};
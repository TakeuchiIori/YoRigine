#pragma once

// Engine
#include "WorldTransform./WorldTransform.h"
#include "Object3D./Object3d.h"
#include "CollisionTypeIdDef.h"
#include "Systems/Camera/Camera.h"
#include "../Graphics/Drawer/LineManager/Line.h"
#include "Loaders/Json/JsonManager.h"
#include "CollisionDirection.h"

// Math
#include "Vector3.h"
#include "Matrix4x4.h"

// コライダーの基底クラス（共通処理とコールバック管理を行う）
// SphereCollider / AABBCollider / OBBCollider などの基盤となるクラス
class BaseCollider {
protected:
	///************************* 基本処理 *************************///

	// 初期化
	// 継承先から呼び出して共通設定を行う
	void Initialize();

public:
	///************************* デストラクタ *************************///

	using CollisionCallback = std::function<void(BaseCollider* self, BaseCollider* other)>;
	using DirectionalCollisionCallback = std::function<void(BaseCollider* self, BaseCollider* other, HitDirection dir)>;

	virtual ~BaseCollider();

public:
	///************************* コールバック設定と呼び出し *************************///

	// 衝突開始時のコールバック登録
	void SetOnEnterCollision(CollisionCallback cb) { enterCallback_ = cb; }

	// 衝突中のコールバック登録
	void SetOnCollision(CollisionCallback cb) { collisionCallback_ = cb; }

	// 衝突終了時のコールバック登録
	void SetOnExitCollision(CollisionCallback cb) { exitCallback_ = cb; }

	// 衝突方向を持つコールバック登録（前後左右のヒット判定など）
	void SetOnDirectionCollision(DirectionalCollisionCallback cb) { directionCallback_ = cb; }

	void SetOnEnterDirectionCollision(DirectionalCollisionCallback cb) { enterDirectionCallback_ = cb; }

	// 衝突開始時のコールバック呼び出し
	void CallOnEnterCollision(BaseCollider* other) {
		if (enterCallback_) enterCallback_(this, other);
	}

	// 衝突中のコールバック呼び出し
	void CallOnCollision(BaseCollider* other) {
		if (collisionCallback_) collisionCallback_(this, other);
	}

	// 衝突終了時のコールバック呼び出し
	void CallOnExitCollision(BaseCollider* other) {
		if (exitCallback_) exitCallback_(this, other);
	}

	// 衝突方向付きのコールバック呼び出し
	void CallOnDirectionCollision(BaseCollider* other, HitDirection dir) {
		if (directionCallback_) directionCallback_(this, other, dir);
	}

	// 衝突開始時の方向付きコールバック呼び出し
	void CallOnEnterDirectionCollision(BaseCollider* other, HitDirection dir) {
		if (enterDirectionCallback_) enterDirectionCallback_(this, other, dir);
	}

public:
	///************************* 継承クラスで実装する処理 *************************///

	// 中心座標取得
	virtual Vector3 GetCenterPosition() const = 0;

	// ワールドトランスフォーム取得
	virtual const WorldTransform& GetWorldTransform() = 0;

	// 回転角度取得（オイラー角）
	virtual Vector3 GetEulerRotation() const = 0;

	// JSONから初期化情報を読み込む
	virtual void InitJson(YoRigine::JsonManager* jsonManager) = 0;

public:
	///************************* アクセッサ *************************///

	// コライダータイプID取得
	uint32_t GetTypeID() const { return typeID_; }

	// コライダータイプID設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

	// カメラ設定（デバッグ表示などで使用）
	void SetCamera(Camera* camera) { camera_ = camera; }

	// ワールドトランスフォーム設定
	void SetWT(const WorldTransform* worldTransform) { wt_ = worldTransform; }

	// 当たり判定の有効フラグ設定
	void SetCollisionEnabled(bool enabled) { isCollisionEnabled_ = enabled; }

	// 当たり判定の有効状態取得
	bool IsCollisionEnabled() const { return isCollisionEnabled_; }

	// カメラ外チェックフラグ取得
	bool IsCheckOutsideCamera() const { return checkOutsideCamera; }

	// コライダー全体の有効状態取得
	bool GetIsActive() const { return isActive_; }

	// コライダー全体の有効状態設定
	void SetActive(bool isActive) { isActive_ = isActive; }

protected:
	///************************* 継承クラス用変数 *************************///

	// コライダー可視化ライン描画用
	std::unique_ptr<Line> line_ = nullptr;

	// 所属オブジェクトのワールドトランスフォーム
	const WorldTransform* wt_ = nullptr;

	// 衝突タイプ識別ID（CollisionTypeIdDefで定義）
	uint32_t typeID_ = 0u;

public:
	///************************* 設定フラグ *************************///

	// 当たり判定を有効化
	bool isCollisionEnabled_ = true;

	// カメラ外に出た際の判定を無効化する
	bool checkOutsideCamera = true;

	// デバッグ表示用カメラ参照
	Camera* camera_ = nullptr;

private:
	///************************* 内部管理変数 *************************///

	// コライダーが有効かどうか
	bool isActive_ = true;

	// 衝突イベント用コールバック群
	CollisionCallback enterCallback_;
	CollisionCallback collisionCallback_;
	CollisionCallback exitCallback_;
	DirectionalCollisionCallback directionCallback_;
	DirectionalCollisionCallback enterDirectionCallback_;

};

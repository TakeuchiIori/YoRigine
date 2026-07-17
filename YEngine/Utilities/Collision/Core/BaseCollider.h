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

#include <functional>
#include <memory>

// コライダー形状の種別。
// dynamic_cast を使わず static_cast で安全にダウンキャストするための識別子。
// 値は配列インデックスとして使うので 0 から連番。
enum class ColliderShape : uint8_t {
	Sphere  = 0,
	AABB    = 1,
	OBB     = 2,
	Capsule = 3,
	Count
};

// コライダーの基底クラス（共通処理とコールバック管理を行う）
// SphereCollider / AABBCollider / OBBCollider / CapsuleCollider の基盤となるクラス
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

	// 更新
	virtual void Update() = 0;
public:
	///************************* アクセッサ *************************///

	// コライダータイプID取得
	uint32_t GetTypeID() const { return typeID_; }

	// コライダータイプID設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

	// コライダーから所有者を取り出すための軽い紐付け。
	// CollisionManager は型を知らないまま衝突を配送し、ゲーム側が必要な時だけ
	// BattleEnemy / MagicAttackInstance などへ戻す。BaseObject 前提にしないため raw に留める。
	void SetOwnerRaw(void* owner) { ownerRaw_ = owner; }
	template <typename T>
	T* GetOwnerAs() const { return static_cast<T*>(ownerRaw_); }

	// 形状種別取得 (CollisionManager のディスパッチで使用)
	ColliderShape GetShape() const { return shape_; }

	// カメラ設定（デバッグ表示などで使用）
	void SetCamera(Camera* camera) { camera_ = camera; }

	// ワールドトランスフォーム設定
	WorldTransform* GetWT() { return wt_; }
	void SetWT(WorldTransform* worldTransform) { wt_ = worldTransform; }

	// 当たり判定の有効フラグ設定
	void SetCollisionEnabled(bool enabled) { isCollisionEnabled_ = enabled; }

	// 当たり判定の有効状態取得
	bool IsCollisionEnabled() const { return isCollisionEnabled_; }

	// カメラ外チェックフラグ取得
	bool IsCheckOutsideCamera() const { return checkOutsideCamera; }

	// カメラ外チェックフラグ設定
	// true  : 視錐台外に出たら CollisionManager の BroadPhase 登録をスキップ (既定)
	// false : 視錐台外でも常に当たり判定を回す (Player / ボス / 落下死トリガーなど)
	void SetCheckOutsideCamera(bool enable) { checkOutsideCamera = enable; }

	// コライダー全体の有効状態取得
	bool GetIsActive() const { return isActive_; }

	// コライダー全体の有効状態設定
	void SetActive(bool isActive) { isActive_ = isActive; }

	// 静的オブジェクトの設定
	void SetIsStatic(bool isStatic) { isStatic_ = isStatic; }
	bool GetIsStatic() const { return isStatic_; }

	// めり込みの有効フラグ設定
	void SetEnablePenetration(bool enable) { enablePenetration_ = enable; }
	bool GetEnablePenetration() const { return enablePenetration_; }

	// 押し戻しのY成分を無効化する（地面上で動くキャラ同士が重なった際に
	// 上下に浮き上がったりガタつくのを防ぐ。水平方向のみ押し戻す）
	void SetLockPenetrationY(bool lock) { lockPenetrationY_ = lock; }
	bool GetLockPenetrationY() const { return lockPenetrationY_; }

	// 質量の設定
	void SetMass(float mass) { mass_ = mass; }
	float GetMass() const { return mass_; }

	// 速度の設定
	void SetVelocity(const Vector3& velocity) { velocity_ = velocity; }
	Vector3 GetVelocity() const { return velocity_; }

	// ─── レイヤーマスク (Unity 風) ──────────────────────────────
	// layerBits_     : このコライダー自身がどの層に属するか (通常は1ビット, 複数ビット可)
	// collisionMask_ : どの層と当たるか (ビット ON で反応)
	// 衝突条件: (a.layer & b.mask) != 0 && (b.layer & a.mask) != 0
	void SetLayerBits(uint32_t bits)        { layerBits_     = bits; }
	void SetLayerBitIndex(uint32_t bit)     { layerBits_     = (1u << bit); }
	void SetCollisionMask(uint32_t mask)    { collisionMask_ = mask; }
	void AddCollisionLayerBit(uint32_t bit) { collisionMask_ |= (1u << bit); }
	void RemoveCollisionLayerBit(uint32_t bit){ collisionMask_ &= ~(1u << bit); }
	uint32_t GetLayerBits() const     { return layerBits_; }
	uint32_t GetCollisionMask() const { return collisionMask_; }

	// 連続衝突判定 (CCD) のオン/オフ
	// 高速移動して薄壁をすり抜ける可能性のあるオブジェクトでオンにする。
	// 既定 false (パフォーマンス優先)。
	void SetCCDEnabled(bool enable) { enableCCD_ = enable; }
	bool IsCCDEnabled() const       { return enableCCD_; }

	// CCD 用 前フレーム中心位置 (CollisionManager が管理)
	void           SetPreviousCenter(const Vector3& p) { previousCenter_ = p; hasPreviousCenter_ = true; }
	const Vector3& GetPreviousCenter() const           { return previousCenter_; }
	bool           HasPreviousCenter() const           { return hasPreviousCenter_; }
	void           ClearPreviousCenter()               { hasPreviousCenter_ = false; }
protected:
	///************************* 継承クラス用変数 *************************///

	// コライダー可視化ライン描画用
	std::unique_ptr<Line> line_ = nullptr;

	// 所属オブジェクトのワールドトランスフォーム
	WorldTransform* wt_ = nullptr;

	// 衝突タイプ識別ID（CollisionTypeIdDefで定義）
	uint32_t typeID_ = 0u;
	void* ownerRaw_ = nullptr;

	// 形状種別 (派生クラスで Initialize() 時にセット)
	ColliderShape shape_ = ColliderShape::Sphere;

	///************************* 設定フラグ *************************///

	// 当たり判定を有効化
	bool isCollisionEnabled_ = true;

	// カメラ外に出た際の判定を無効化する
	bool checkOutsideCamera = true;

	// 静的オブジェクトかどうか
	bool isStatic_ = false;

	// めり込みを行うかどうか
	bool enablePenetration_ = true;

	// 押し戻しのY成分をロックするか（地面上キャラ用）
	bool lockPenetrationY_ = false;

	// デバッグ表示用カメラ参照
	Camera* camera_ = nullptr;

	// コライダーに設定する質量
	float mass_ = 1.0f;

	Vector3 velocity_ {};

	// レイヤーマスク (既定: 全層に属し、全層と当たる → 後方互換)
	uint32_t layerBits_     = 0xFFFFFFFFu;
	uint32_t collisionMask_ = 0xFFFFFFFFu;

	// CCD (連続衝突判定) 関連
	bool    enableCCD_         = false;
	bool    hasPreviousCenter_ = false;
	Vector3 previousCenter_    = {};

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

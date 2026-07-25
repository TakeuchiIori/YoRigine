#pragma once

// Engine
#include "Sprite/Sprite.h"
#include "Loaders/Json/JsonManager.h"

// Math
#include "Vector2.h"
#include "Vector3.h"
#include "MathFunc.h"

// 前方宣言
class Player;
class BaseCollider;

// ============================================================
// ロックオンUI クラス
//
// 依存関係:
//   Player → FollowCamera → GetLockedTarget() → BaseCollider
//   BaseCollider::GetWorldPosition() でワールド座標を取得し、
//   Coordinate::WorldToScreen() でスクリーン座標に変換して描画する
//
// BattleScene から Initialize(player_) を呼んで使う
// ============================================================
class LockOnUI
{
public:
	// ============================================================
	// 基本関数
	// ============================================================

	LockOnUI() = default;
	~LockOnUI() = default;

	/// <summary>
	/// 初期化（BattleScene から player_ を渡す）
	/// </summary>
	void Initialize(Player* player);

	/// <summary>
	/// 毎フレーム更新
	/// ロックオン対象がいる場合のみスプライトを更新する
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();


	// ============================================================
	// アクセッサ
	// ============================================================
	void SetIsVisible(bool visible) { isVisible_ = visible; }

private:
	// ============================================================
	// 内部処理
	// ============================================================

	/// <summary>
	/// ロックオン対象のワールド座標をスクリーン座標に変換し
	/// スプライトの位置・サイズを更新する
	/// </summary>
	void UpdateScreenPosition();

	/// <summary>
	/// Json パラメータの登録
	/// </summary>
	void InitJson();

private:
	// ============================================================
	// 参照
	// ============================================================

	Player* player_ = nullptr;			// FollowCamera の取得元

	// ============================================================
	// スプライト
	// ============================================================

	std::unique_ptr<YoRigine::Sprite> sprite_ = nullptr;
	Vector2 defaultScale_ = {};			// テクスチャ元サイズ（スケーリングの基準）

	// ============================================================
	// パラメータ（Json で調整可能）
	// ============================================================

	// ロックオン対象の頭上に表示するためのオフセット
	Vector3 offset_ = { 0.0f, 2.0f, 0.0f };

	// 距離スケーリングの基準距離（EnemyAlert と同じ定数）
	float kReferenceDistance_ = 20.0f;

	// スプライトの回転速度（ラジアン/秒）
	float rotateSpeed_ = 2.0f;

	// 現在の回転角（Z 軸）
	float currentRotation_ = 0.0f;

	// ============================================================
	// 状態
	// ============================================================

	bool isVisible_ = false;			// ロックオン中のみ true

	// ============================================================
	// Json
	// ============================================================

	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
};
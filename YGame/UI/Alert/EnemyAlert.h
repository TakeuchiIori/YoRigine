#pragma once
#include "Systems/UI/UIBase.h"
#include "Loaders/Json/JsonManager.h"


class FieldEnemy;
class Camera;
// ============================================================
// プレイヤー発見時のUIクラス
// ============================================================
class EnemyAlert
{

public:
	// ============================================================
	// 基本的なクラス
	// ============================================================

	EnemyAlert(const FieldEnemy* enemy, Camera* camera);

	void Initialize();

	void Update();

	void Draw();


	// ============================================================
	// アクセッサ
	// ============================================================

	void SetIsVisible(bool isVisible) { this->isVisible_ = isVisible; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void InitJson();

	// ワールド座標をスクリーン座標に変換してUIの位置を更新
	void WorldToScreen();

private:
	const FieldEnemy* targetEnemy_ = nullptr;				// 追従対象の敵
	Camera* camera_ = nullptr;								// カメラ
	std::unique_ptr<Sprite> alertUI_ = nullptr;								// 実際のUI画像
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;	// 保存用のJsonManager

	// 座標変換用の変数
	Vector3 offset_{};
	Vector3 worldPos_{};
	float kReferenceDistance_ = 20.0f;						// 距離によるスケーリングの基準距離

	// アニメーションをしながら表示するための変数
	bool isVisible_ = true;
	Vector2 defaultScale_{};
	Vector2 targetScale_ = {};
};


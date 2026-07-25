#include "EnemyAlert.h"
#include <MathFunc.h>
#include "Systems/UI/UIManager.h"
#include <Easing.h>

#include "Enemy/FieldEnemy/FieldEnemy.h"

// ============================================================
// コンストラクタ
// ============================================================
EnemyAlert::EnemyAlert(const FieldEnemy* enemy, YoRigine::Camera* camera){
	targetEnemy_ = enemy;
	camera_ = camera;
}

// ============================================================
// 初期化
// ============================================================
void EnemyAlert::Initialize() {
	alertUI_ = std::make_unique<YoRigine::Sprite>();
	alertUI_->Initialize("Resources/Textures/GameScene/AlertIcon.png");
	alertUI_->SetAnchorPoint({ 0.5f, 0.5f });
	defaultScale_ = alertUI_->GetTextureSize();
	InitJson();
}

// ============================================================
// 更新
// ============================================================
void EnemyAlert::Update() {

	//// 見つかった瞬間の処理
	//if (isVisible_) {

	//}
	//else {

	//}
	WorldToScreen();
}

// ============================================================
// 描画
// ============================================================
void EnemyAlert::Draw() {
	if (alertUI_ && isVisible_) {
		alertUI_->Draw();
	}
}

// ============================================================
// Json初期化
// ============================================================
void EnemyAlert::InitJson(){
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("EnemyAlert", "Resources/Json/UI");
	jsonManager_->SetCategory("UI");
	jsonManager_->SetSubCategory("EnemyAlert");

	//------------------------------------------------------------
	// メイン情報
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("メイン情報");
	jsonManager_->Register("ターゲットスケール", &targetScale_);
	jsonManager_->Register("オフセット", &offset_);
}

// ============================================================
// ワールド座標をスクリーン座標に変換してUIの位置を更新
// ============================================================
void EnemyAlert::WorldToScreen(){
	Vector3 worldPos = targetEnemy_->GetPosition() + offset_;
	auto projection = Coordinate::WorldToScreen(worldPos, camera_->GetViewProjectionMatrix(),
		kReferenceDistance_, 2.0f);

	// ワールド座標がカメラの前にある場合のみ
	if (projection.has_value()) {
		Vector2 size = defaultScale_ * projection->distanceScale;
		alertUI_->SetSize(size);
		alertUI_->SetTranslate(projection->screenPos);
		alertUI_->Update();
	}
}

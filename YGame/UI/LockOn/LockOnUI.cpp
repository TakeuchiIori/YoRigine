#include "LockOnUI.h"

// Engine
#include "Systems/GameTime/GameTime.h"

// Player / Camera
#include "Player/Player.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"

// ============================================================
// 初期化
// ============================================================
void LockOnUI::Initialize(Player* player)
{
	player_ = player;

	// ─── スプライト生成 ──────────────────────────────────────
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/GameScene/LockOn.png");
	sprite_->SetAnchorPoint({ 0.5f, 0.5f });	// 中央アンカー（対象の真上に表示）

	defaultScale_ = sprite_->GetTextureSize();	// 元サイズを保持（距離スケーリング用）

	// ─── Json ────────────────────────────────────────────────
	InitJson();
}

// ============================================================
// 更新
// ============================================================
void LockOnUI::Update()
{
	// ─── ロックオン対象の確認 ────────────────────────────────
	//   Player → FollowCamera → GetLockedTarget() の経路
	FollowCamera* followCamera = player_->GetFollowCamera();
	if (!followCamera) {
		isVisible_ = false;
		return;
	}

	BaseCollider* target = followCamera->GetLockedTarget();
	if (!target) {
		isVisible_ = false;
		return;
	}

	// ─── ロックオン中 ────────────────────────────────────────
	isVisible_ = true;

	// スプライト回転（ロックオンサイトをゆっくり回す演出）
	const float deltaTime = YoRigine::GameTime::GetDeltaTime();
	currentRotation_ += rotateSpeed_ * deltaTime;
	sprite_->SetRotate({ 0.0f, 0.0f, currentRotation_ });

	// ワールド座標 → スクリーン座標 変換 & 位置更新
	UpdateScreenPosition();
}

// ============================================================
// 描画
// ============================================================
void LockOnUI::Draw()
{
	if (!isVisible_) return;
	sprite_->Draw();
}

// ============================================================
// ワールド→スクリーン変換
// ============================================================
void LockOnUI::UpdateScreenPosition()
{
	// ─── カメラと対象を取得 ──────────────────────────────────
	FollowCamera* followCamera = player_->GetFollowCamera();
	BaseCollider* target = followCamera->GetLockedTarget();

	// 対象のワールド座標 + 頭上オフセット
	Vector3 worldPos = target->GetWT()->translate_ + offset_;

	// ─── Coordinate::WorldToScreen で変換 ───────────────────
	//   EnemyAlert と同じ手順
	auto projection = Coordinate::WorldToScreen(
		worldPos,
		followCamera->GetViewProjectionMatrix(),
		kReferenceDistance_,
		2.0f
	);

	// カメラの後ろにある場合は非表示
	if (!projection.has_value()) {
		isVisible_ = false;
		return;
	}

	// ─── スプライトに反映 ────────────────────────────────────
	// 距離に応じてサイズをスケーリング（遠いほど小さく）
	Vector2 scaledSize = defaultScale_ * projection->distanceScale;
	sprite_->SetSize(scaledSize);
	sprite_->SetTranslate(projection->screenPos);

	sprite_->Update();
}

// ============================================================
// Json 初期化
// ============================================================
void LockOnUI::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("LockOnUI", "Resources/Json/UI");
	jsonManager_->SetCategory("UI");
	jsonManager_->SetSubCategory("LockOnUI");

	jsonManager_->SetTreePrefix("メイン情報");
	jsonManager_->Register("オフセット", &offset_);
	jsonManager_->Register("基準距離", &kReferenceDistance_);
	jsonManager_->Register("回転速度(rad/s)", &rotateSpeed_);
}
#include "PlayerShield.h"
#include "Player/Player.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/Core/CollisionManager.h"
#include "MathFunc.h"
#include "Systems/GameTime/GameTime.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
Matrix4x4 RemoveScaleFromMatrix(const Matrix4x4& matrix) {
	Matrix4x4 result = matrix;

	for (int row = 0; row < 3; ++row) {
		Vector3 axis{
			result.m[row][0],
			result.m[row][1],
			result.m[row][2]
		};

		const float length = Length(axis);
		if (length > 0.0001f) {
			result.m[row][0] /= length;
			result.m[row][1] /= length;
			result.m[row][2] /= length;
		}
	}

	return result;
}
}

// ============================================================
// デストラクタ
// ============================================================
PlayerShield::~PlayerShield() {
	obbCollider_->~OBBCollider();
}

// ============================================================
// 初期化処理
// ============================================================
void PlayerShield::Initialize(YoRigine::Camera* camera) {

	camera_ = camera;
	// ------------------------------------------------------------
	// モデル初期化
	// ------------------------------------------------------------
	obj_ = std::make_unique<YoRigine::Object3d>();
	obj_->Initialize();
	obj_->SetModel("Shield_Heater.obj");
	wt_.Initialize();

	// ------------------------------------------------------------
	// プレイヤーの「手ジョイント」を探索して、盾を接続
	// ------------------------------------------------------------
	FindHandJointIndex();
	wt_.parent_ = nullptr;

	// ------------------------------------------------------------
	// コライダー・Json・パーティクル初期化
	// ------------------------------------------------------------
	InitCollision();
	InitJson();
}

// ============================================================
// 毎フレーム更新処理
// ============================================================
void PlayerShield::Update() {
	if (obj3d_) {
		SetPlayerWeaponPosition();
	}
	obbCollider_->Update();
}

// ============================================================
// プレイヤーの手ジョイントを探索
// ============================================================
void PlayerShield::FindHandJointIndex() {
	if (!obj3d_) return;

	auto* skeleton = obj3d_->GetModel()->GetSkeleton();
	if (!skeleton) return;

	auto& jointMap = skeleton->GetJointMap();
	auto it = jointMap.find(handJointName_);

	if (it != jointMap.end()) {
		handleIndex_ = it->second;
		isValidJoint_ = true;
	}
	else {
		isValidJoint_ = false;

		std::vector<std::string> handCandidates = {
			"mixamorig:RightHand", "mixamorig:LeftHand",
			"RightHand", "LeftHand",
			"Hand_R", "Hand_L"
		};

		for (const auto& candidate : handCandidates) {
			auto candidateIt = jointMap.find(candidate);
			if (candidateIt != jointMap.end()) {
				handJointName_ = candidate;
				handleIndex_ = candidateIt->second;
				isValidJoint_ = true;
				break;
			}
		}
	}
}

// ============================================================
// 手のジョイント位置に盾を追従させる
// ============================================================
void PlayerShield::SetPlayerWeaponPosition() {
	if (!obj3d_ || !isValidJoint_) return;

	wt_.translate_ = offsetPos_;
	wt_.rotate_ = offsetRot_;
	wt_.scale_ = offsetScale_;

	YoRigine::WorldTransform& handWT = obj3d_
		->GetModel()
		->GetSkeleton()
		->GetJoints()[handleIndex_]
		.GetWorldTransform();

	const Matrix4x4 handNoScale = RemoveScaleFromMatrix(handWT.matWorld_);
	const Matrix4x4 shieldMatrix = MakeAffineMatrix(wt_.scale_, wt_.rotate_, wt_.translate_);
	wt_.matWorld_ = Multiply(shieldMatrix, handNoScale);
}

// ============================================================
// 描画処理
// ============================================================
void PlayerShield::Draw() {
	if (!isVisible_) return;
	if (obj_) {
		obj_->Draw(camera_, wt_);
	}
}

// ============================================================
// 影の描画
// ============================================================
void PlayerShield::DrawShadow()
{
	if (!isVisible_) return;
	if (obj_) {
		obj_->DrawShadow(wt_);
	}
}

// ============================================================
// コライダー描画
// ============================================================
void PlayerShield::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

// ============================================================
// 衝突開始時の処理
// ============================================================
void PlayerShield::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// ------------------------------------------------------------
		// ガード判定の実行と結果処理
		// ------------------------------------------------------------
		auto guardResult = player_->GetCombat()->GetGuard()->OnHit(other);

		switch (guardResult) {
		case PlayerGuard::GuardResult::ParrySuccess:
			break;

		case PlayerGuard::GuardResult::GuardSuccess:
			break;

		case PlayerGuard::GuardResult::GuardFail:
			break;
		}
	}
}

// ============================================================
// 衝突中の処理
// ============================================================
void PlayerShield::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		auto guardResult = player_->GetCombat()->GetGuard()->OnHit(other);

		switch (guardResult) {
		case PlayerGuard::GuardResult::ParrySuccess:
			break;

		case PlayerGuard::GuardResult::GuardSuccess:
			break;

		case PlayerGuard::GuardResult::GuardFail:
			break;
		}
	}
}

// ============================================================
// 衝突終了時の処理
// ============================================================
void PlayerShield::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

// ============================================================
// 衝突方向ごとの処理
// ============================================================
void PlayerShield::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

// ============================================================
// 衝突方向開始時の処理
// ============================================================
void PlayerShield::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

// ============================================================
// コライダー初期化
// ============================================================
void PlayerShield::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)
	);

	obbCollider_->SetEnablePenetration(false);
}

// ============================================================
// Json登録処理
// ============================================================
void PlayerShield::InitJson() {
	// ------------------------------------------------------------
	// メイン設定
	// ------------------------------------------------------------
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("PlayerShield", "Resources/Json/Weapon");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("PlayerShield");
	jsonManager_->Register("Translation", &wt_.translate_);
	jsonManager_->Register("Rotate", &wt_.rotate_);
	jsonManager_->Register("Scale", &wt_.scale_);
	jsonManager_->Register("Use Anchor Point", &wt_.useAnchorPoint_);
	jsonManager_->Register("AnchorPoint", &wt_.anchorPoint_);
	jsonManager_->Register("Hand Joint Name", &handJointName_);

	// ------------------------------------------------------------
	// オフセット設定（手元からの補正位置）
	// ------------------------------------------------------------
	jsonManager_->Register("Offset Position", &offsetPos_);
	jsonManager_->Register("Offset Rotation", &offsetRot_);
	jsonManager_->Register("Offset Scale", &offsetScale_);

	// ------------------------------------------------------------
	// コライダー設定
	// ------------------------------------------------------------
	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("PlayerShieldCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

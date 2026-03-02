#include "PlayerShield.h"
#include "Player/Player.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/Core/CollisionManager.h"
#include "MathFunc.h"
#include "Systems/GameTime/GameTime.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// デストラクタ
/// </summary>
PlayerShield::~PlayerShield() {
	obbCollider_->~OBBCollider();
}

/// <summary>
/// 初期化処理
/// </summary>
void PlayerShield::Initialize(Camera* camera) {

	camera_ = camera;
	//------------------------------------------------------------
	// モデル初期化
	//------------------------------------------------------------
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("Shield_Heater.obj");
	obj_->SetEnableEnvironment(true);
	obj_->SetEnvironmentCoefficient(1.0f);
	wt_.Initialize();

	//------------------------------------------------------------
	// プレイヤーの「手ジョイント」を探索して、盾を接続
	//------------------------------------------------------------
	FindHandJointIndex();
	if (isValidJoint_) {
		WorldTransform& handWT = obj3d_
			->GetModel()
			->GetSkeleton()
			->GetJoints()[handleIndex_]
			.GetWorldTransform();
		wt_.parent_ = &handWT;
	}

	//------------------------------------------------------------
	// コライダー・Json・パーティクル初期化
	//------------------------------------------------------------
	InitCollision();
	InitJson();
	testEmitter_ = std::make_unique<ParticleEmitter>("GuardParticle", wt_.translate_, 10);
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void PlayerShield::Update() {
	// 手の位置に追従
	if (obj3d_) {
		SetPlayerWeaponPosition();
	}
	// 当たり判定更新
	obbCollider_->Update();
}

/// <summary>
/// プレイヤーの手ジョイントを探索
/// </summary>
void PlayerShield::FindHandJointIndex() {
	if (!obj3d_) return;

	auto* skeleton = obj3d_->GetModel()->GetSkeleton();
	if (!skeleton) return;

	auto& jointMap = skeleton->GetJointMap();
	auto it = jointMap.find(handJointName_);

	if (it != jointMap.end()) {
		// 設定された名前でヒット
		handleIndex_ = it->second;
		isValidJoint_ = true;
	} else {
		isValidJoint_ = false;

		// フォールバック：代表的な手ジョイント候補を探索
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

/// <summary>
/// 手のジョイント位置に盾を追従させる
/// </summary>
void PlayerShield::SetPlayerWeaponPosition() {
	if (!obj3d_ || !isValidJoint_) return;

	wt_.translate_ = offsetPos_;  // 相対位置
	wt_.rotate_ = offsetRot_;     // 相対回転
	wt_.scale_ = offsetScale_;    // スケール調整
	wt_.UpdateMatrix();           // 行列更新
}

/// <summary>
/// 描画処理
/// </summary>
void PlayerShield::Draw() {
	if (obj_) {
		obj_->Draw(camera_, wt_);
	}
}

void PlayerShield::DrawShadow()
{
	if (obj_) {
		obj_->DrawShadow(wt_);
	}
}

/// <summary>
/// コライダー描画（デバッグ用）
/// </summary>
void PlayerShield::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

/// <summary>
/// 衝突開始時の処理
/// </summary>
void PlayerShield::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		//------------------------------------------------------------
		// ガード判定の実行と結果処理
		//------------------------------------------------------------
		auto guardResult = player_->GetCombat()->GetGuard()->OnHit(other);

		switch (guardResult) {
		case PlayerGuard::GuardResult::ParrySuccess:  // パリィ成功
			break;

		case PlayerGuard::GuardResult::GuardSuccess:  // 通常ガード成功
			testEmitter_->FollowEmit(wt_.translate_, 10);
			
			break;

		case PlayerGuard::GuardResult::GuardFail:     // 失敗（貫通）
			break;
		}
	}
}

/// <summary>
/// 衝突中の処理
/// </summary>
void PlayerShield::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		auto guardResult = player_->GetCombat()->GetGuard()->OnHit(other);

		switch (guardResult) {
		case PlayerGuard::GuardResult::ParrySuccess:
			// パリィ時の演出を追加可能
			break;

		case PlayerGuard::GuardResult::GuardSuccess:
			testEmitter_->FollowEmit(wt_.translate_, 10);
			break;

		case PlayerGuard::GuardResult::GuardFail:
			break;
		}
	}
}

/// <summary>
/// 衝突終了時の処理
/// </summary>
void PlayerShield::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

/// <summary>
/// 衝突方向ごとの処理
/// </summary>
void PlayerShield::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

void PlayerShield::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

/// <summary>
/// コライダー初期化
/// </summary>
void PlayerShield::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)
	);
}

/// <summary>
/// Json登録処理
/// </summary>
void PlayerShield::InitJson() {
	//------------------------------------------------------------
	// メイン設定
	//------------------------------------------------------------
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("PlayerShield", "Resources/Json/Weapon");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("PlayerShield");
	jsonManager_->Register("Translation", &wt_.translate_);
	jsonManager_->Register("Rotate", &wt_.rotate_);
	jsonManager_->Register("Scale", &wt_.scale_);
	jsonManager_->Register("Use Anchor Point", &wt_.useAnchorPoint_);
	jsonManager_->Register("AnchorPoint", &wt_.anchorPoint_);
	jsonManager_->Register("Hand Joint Name", &handJointName_);

	//------------------------------------------------------------
	// オフセット設定（手元からの補正位置）
	//------------------------------------------------------------
	jsonManager_->Register("Offset Position", &offsetPos_);
	jsonManager_->Register("Offset Rotation", &offsetRot_);
	jsonManager_->Register("Offset Scale", &offsetScale_);

	//------------------------------------------------------------
	// コライダー設定
	//------------------------------------------------------------
	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("PlayerShieldCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

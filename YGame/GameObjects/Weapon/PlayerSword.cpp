#include "PlayerSword.h"
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
PlayerSword::~PlayerSword() {
	obbCollider_->~OBBCollider();
}

/// <summary>
/// 初期化処理
/// </summary>
void PlayerSword::Initialize(Camera* camera) {

	camera_ = camera;
	//------------------------------------------------------------
	// モデル初期化
	//------------------------------------------------------------
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("Sword_Golden.obj");
	obj_->SetEnableEnvironment(true);
	obj_->SetEnvironmentCoefficient(1.0f);
	wt_.Initialize();
	colliderWT_.Initialize();

	//------------------------------------------------------------
	// プレイヤーの手ジョイントを探索し、剣を接続
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

	particleEmitter_ = std::make_unique<ParticleEmitter>("PlayerParticle", wt_.translate_, 5);
	hitParticleEmitter_ = std::make_unique<ParticleEmitter>("PlayerHitParticle", wt_.translate_, 5);
	testEmitter_ = std::make_unique<ParticleEmitter>("TestParticle", wt_.translate_, 10);
}

/// <summary>
/// 更新処理
/// </summary>
void PlayerSword::Update() {
	// 剣を手に追従させ、コライダー位置を更新
	if (obj3d_) {
		SetPlayerWeaponPosition();
		UpdateColliderWorldTransform();
	}

	// 当たり判定更新
	Vector3 handPos = GetHandPosition();
	obbCollider_->Update();
}

/// <summary>
/// 手のジョイントを探索
/// </summary>
void PlayerSword::FindHandJointIndex() {
	if (!obj3d_ || !obj3d_->GetModel()) return;

	auto* skeleton = obj3d_->GetModel()->GetSkeleton();
	if (!skeleton) return;

	auto& jointMap = skeleton->GetJointMap();
	auto it = jointMap.find(handJointName_);

	if (it != jointMap.end()) {
		// 指定ジョイントが存在
		handleIndex_ = it->second;
		isValidJoint_ = true;
	}
	else {
		isValidJoint_ = false;

		// フォールバック:一般的な手ジョイント名候補を探索
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
/// 手ジョイントの相対位置に剣を配置
/// </summary>
void PlayerSword::SetPlayerWeaponPosition() {
	if (!obj3d_ || !isValidJoint_) return;

	wt_.translate_ = offsetPos_;  // 相対位置
	wt_.rotate_ = offsetRot_;     // 相対回転
	wt_.scale_ = offsetScale_;    // 相対スケール
	wt_.UpdateMatrix();           // 行列更新
}

/// <summary>
/// コライダー用のワールド変換を更新
/// </summary>
void PlayerSword::UpdateColliderWorldTransform() {
	if (!obj3d_ || !isValidJoint_) return;

	// 手のジョイントのワールド行列を取得
	WorldTransform& handWT = obj3d_
		->GetModel()
		->GetSkeleton()
		->GetJoints()[handleIndex_]
		.GetWorldTransform();

	// 剣の相対変換を行列化
	Matrix4x4 weaponMatrix = MakeAffineMatrix(wt_.scale_, wt_.rotate_, wt_.translate_);

	// 手と剣の行列を合成して最終的な位置を求める
	Matrix4x4 finalMatrix = Multiply(weaponMatrix, handWT.matWorld_);

	// コライダー用に結果を適用
	colliderWT_.matWorld_ = finalMatrix;
	colliderWT_.translate_ = ExtractTranslation(finalMatrix);
}

/// <summary>
/// 手のワールド位置を取得
/// </summary>
Vector3 PlayerSword::GetHandPosition() {
	if (!isValidJoint_) return Vector3{};

	auto* obj = obj3d_;
	if (!obj || !obj->GetModel()) return Vector3{};

	auto* skeleton = obj->GetModel()->GetSkeleton();
	if (!skeleton) return Vector3{};

	// 手ジョイントのワールド変換から位置を抽出
	WorldTransform& handWT = skeleton->GetJoints()[handleIndex_].GetWorldTransform();
	return Vector3(handWT.matWorld_.m[3][0], handWT.matWorld_.m[3][1], handWT.matWorld_.m[3][2]);
}

/// <summary>
/// 行列から平行移動成分を抽出
/// </summary>
Vector3 PlayerSword::ExtractTranslation(const Matrix4x4& matrix) {
	return Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}

Vector3 PlayerSword::GetWowldPosition()
{
	Vector3 wp;
	wp.x = wt_.matWorld_.m[3][0];
	wp.y = wt_.matWorld_.m[3][1];
	wp.z = wt_.matWorld_.m[3][2];
	return wp;
}

/// <summary>
/// 描画処理
/// </summary>
void PlayerSword::Draw() {
	if (obj_) {
		obj_->Draw(camera_, wt_);
	}
}

void PlayerSword::DrawShadow()
{
	if (obj_) {
		obj_->DrawShadow(wt_);
	}
}

/// <summary>
/// コライダー描画(デバッグ用)
/// </summary>
void PlayerSword::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

/// <summary>
/// 衝突開始時の処理
/// </summary>
void PlayerSword::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		//------------------------------------------------------------
		// 敵にヒット時:エフェクト発生 + CC回復処理 + カメラシェイク
		//------------------------------------------------------------
		Vector3 hitPos = other->GetWorldTransform().translate_;
		hitPos.y += 1.5f;
		//hitParticleEmitter_->FollowEmit(hitPos, 5);
		//particleEmitter_->FollowEmit(hitPos, 30);
		player_->GetCombat()->GetCombo()->RecoverCC(2);

	}
}

/// <summary>
/// 衝突中の処理
/// </summary>
void PlayerSword::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

/// <summary>
/// 衝突終了時の処理
/// </summary>
void PlayerSword::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

/// <summary>
/// 衝突方向別の処理
/// </summary>
void PlayerSword::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

void PlayerSword::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

/// <summary>
/// コライダー初期化
/// </summary>
void PlayerSword::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &colliderWT_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)
	);

	// 初期状態では当たり判定無効
	obbCollider_->isCollisionEnabled_ = false;
}

/// <summary>
/// Json設定初期化
/// </summary>
void PlayerSword::InitJson() {
	//------------------------------------------------------------
	// メイン情報
	//------------------------------------------------------------
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("PlayerSword", "Resources/Json/Weapon");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("PlayerSword");
	jsonManager_->Register("Translation", &wt_.translate_);
	jsonManager_->Register("Rotate", &wt_.rotate_);
	jsonManager_->Register("Scale", &wt_.scale_);
	jsonManager_->Register("Use Anchor Point", &wt_.useAnchorPoint_);
	jsonManager_->Register("AnchorPoint", &wt_.anchorPoint_);
	jsonManager_->Register("Hand Joint Name", &handJointName_);

	//------------------------------------------------------------
	// オフセット情報(位置・回転・スケール)
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("OffSet");
	jsonManager_->Register("Offset Position", &offsetPos_);
	jsonManager_->Register("Offset Rotation", &offsetRot_);
	jsonManager_->Register("Offset Scale", &offsetScale_);

	//------------------------------------------------------------
	// カラー情報
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("Color");
	jsonManager_->Register("", &obj_->GetColor());

	//------------------------------------------------------------
	// コライダー情報
	//------------------------------------------------------------
	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("PlayerSwordCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}
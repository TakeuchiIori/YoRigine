#include "PlayerSword.h"
#include "Player/Player.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/Core/CollisionManager.h"
#include "MathFunc.h"
#include "Systems/GameTime/GameTime.h"
#include "Particle/EffectHandle.h"
#include <algorithm>

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
// 初期化処理
// ============================================================
void PlayerSword::Initialize(YoRigine::Camera* camera) {

	camera_ = camera;
	// ------------------------------------------------------------
	// モデル初期化
	// ------------------------------------------------------------
	obj_ = std::make_unique<YoRigine::Object3d>();
	obj_->Initialize();
	obj_->SetModel("Sword_Golden.obj");
	wt_.Initialize();
	colliderWT_.Initialize();

	// 魔法スタイル時の見た目（杖）。剣と同じ手ジョイントに追従させる。
	staffObj_ = std::make_unique<YoRigine::Object3d>();
	staffObj_->Initialize();
	staffObj_->SetModel("Staff.obj");
	staffWT_.Initialize();

	// ------------------------------------------------------------
	// プレイヤーの手ジョイントを探索し、剣を接続
	// ------------------------------------------------------------
	FindHandJointIndex();
	wt_.parent_ = nullptr;

	// ------------------------------------------------------------
	// コライダー・Json初期化
	// ------------------------------------------------------------
	InitCollision();
	InitJson();


	// ------------------------------------------------------------
	// エフェクト関連の初期化（YParticle）
	// エフェクト定義は JSON で管理。ここでは宣言のみ。
	// 実際の放出は OnHit() や OnAttack() で EffectHandle::PlayOneShot() を使う。
	// ------------------------------------------------------------

	trailEmitter_ = std::make_unique<YoRigine::TrailMeshEmitter>();
	trailEmitter_->SetCamera(camera_);
	trailEmitter_->LoadAsset("Resources/Json/VfxMesh/NewEffect2.json");
}

// ============================================================
// 更新処理
// ============================================================
void PlayerSword::Update() {
	if (obj3d_) {
		SetPlayerWeaponPosition();
		UpdateColliderWorldTransform();
	}

	Vector3 rootW = Transform(localRoot, wt_.matWorld_);
	Vector3 tipW = Transform(localTip, wt_.matWorld_);
	trailEmitter_->AddPoint(tipW, rootW);
	trailEmitter_->Update(YoRigine::GameTime::GetDeltaTime(YoRigine::TimeChannel::Vfx));

	// Vector3 handPos = GetHandPosition(); // 使っていなければ削除でOK
	obbCollider_->Update();
}

// ============================================================
// 手のジョイントを探索
// ============================================================
void PlayerSword::FindHandJointIndex() {
	if (!obj3d_ || !obj3d_->GetModel()) return;

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
// 手ジョイントの相対位置に武器（剣・杖）を配置
// ============================================================
void PlayerSword::SetPlayerWeaponPosition() {
	if (!obj3d_ || !isValidJoint_) return;

	// 剣（近接コライダー・トレイルは wt_ に紐づくので常に更新しておく）
	wt_.translate_ = offsetPos_;
	wt_.rotate_ = offsetRot_;
	wt_.scale_ = offsetScale_;
	wt_.matWorld_ = ComposeWeaponMatrixFromHand(offsetPos_, offsetRot_, offsetScale_);

	// 杖（魔法スタイル時に描画。中央原点前提のオフセットで手に合わせる）
	staffWT_.translate_ = staffOffsetPos_;
	staffWT_.rotate_ = staffOffsetRot_;
	staffWT_.scale_ = staffOffsetScale_;
	staffWT_.matWorld_ = ComposeWeaponMatrixFromHand(staffOffsetPos_, staffOffsetRot_, staffOffsetScale_);
}

// ============================================================
// 手ジョイントを基準にオフセットからワールド行列を組む共通処理
// ============================================================
Matrix4x4 PlayerSword::ComposeWeaponMatrixFromHand(const Vector3& pos, const Vector3& rot, const Vector3& scale) const {
	YoRigine::WorldTransform& handWT = obj3d_
		->GetModel()
		->GetSkeleton()
		->GetJoints()[handleIndex_]
		.GetWorldTransform();

	const Matrix4x4 handNoScale = RemoveScaleFromMatrix(handWT.matWorld_);
	const Matrix4x4 weaponMatrix = MakeAffineMatrix(scale, rot, pos);
	return Multiply(weaponMatrix, handNoScale);
}

// ============================================================
// コライダー用のワールド変換を更新
// ============================================================
void PlayerSword::UpdateColliderWorldTransform() {
	if (!obj3d_ || !isValidJoint_) return;

	finalMatrix_ = wt_.matWorld_;

	colliderWT_.matWorld_ = finalMatrix_;
	colliderWT_.translate_ = ExtractTranslation(finalMatrix_);
}

// ============================================================
// 手のワールド位置を取得
// ============================================================
Vector3 PlayerSword::GetHandPosition() {
	if (!isValidJoint_) return Vector3{};

	auto* obj = obj3d_;
	if (!obj || !obj->GetModel()) return Vector3{};

	auto* skeleton = obj->GetModel()->GetSkeleton();
	if (!skeleton) return Vector3{};

	YoRigine::WorldTransform& handWT = skeleton->GetJoints()[handleIndex_].GetWorldTransform();
	return Vector3(handWT.matWorld_.m[3][0], handWT.matWorld_.m[3][1], handWT.matWorld_.m[3][2]);
}

// ============================================================
// 行列から平行移動成分を抽出
// ============================================================
Vector3 PlayerSword::ExtractTranslation(const Matrix4x4& matrix) {
	return Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}

// ============================================================
// ワールド座標取得
// ============================================================
Vector3 PlayerSword::GetWowldPosition()
{
	Vector3 wp;
	wp.x = wt_.matWorld_.m[3][0];
	wp.y = wt_.matWorld_.m[3][1];
	wp.z = wt_.matWorld_.m[3][2];
	return wp;
}

// ============================================================
// 描画処理
// ============================================================
void PlayerSword::Draw() {
	// 魔法スタイルなら杖、それ以外は剣を描画する。
	if (isMagicVisual_) {
		if (staffObj_) staffObj_->Draw(camera_, staffWT_);
	}
	else {
		if (obj_) obj_->Draw(camera_, wt_);
	}
}

// ============================================================
// 影の描画
// ============================================================
void PlayerSword::DrawShadow()
{
	if (isMagicVisual_) {
		if (staffObj_) staffObj_->DrawShadow(staffWT_);
	}
	else {
		if (obj_) obj_->DrawShadow(wt_);
	}
}

// ============================================================
// コライダー描画
// ============================================================
void PlayerSword::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

// ============================================================
// 剣の残像エフェクト描画
// ============================================================
void PlayerSword::DrawVfx() {
	// 剣の軌跡は剣スタイルのときだけ描画する（杖表示中は出さない）。
	if (isMagicVisual_) return;
	if (trailEmitter_) {
		trailEmitter_->Draw();
	}
}

// ============================================================
// 衝突開始時の処理
// ============================================================
void PlayerSword::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// ------------------------------------------------------------
		// 敵にヒット時:エフェクト発生 + CC回復処理
		// ------------------------------------------------------------
		Vector3 hitPos = other->GetWorldTransform().translate_;
		hitPos.y += 1.5f;

		// "EnemyHit" は複数Systemを束ねた Group。EffectHandle が名前で解決して発火する。
		hitEffect_ = EffectHandle::PlayOneShot("EnemyHit", hitPos, 10);
		player_->GetCombat()->GetCombo()->RecoverCC(2);

		// ------------------------------------------------------------
		// ヒットイベントを PlayerCombo に通知する。
		// カメラワーク・ヒットストップ・シェイクは AttackingCombatState の
		// SetAttackHitCallback で AttackData 駆動で発火される。
		// 武器クラスはヒット検出と通知だけを担当する。
		// ------------------------------------------------------------
		player_->GetCombat()->GetCombo()->NotifyAttackHit(other, hitPos);

		// 画面外（見切れ）の敵にヒットしたら、敵を捉え直す決めカメラを発火する。
		// 画面外判定・発火条件・クールダウンは PlayerCamera が一元管理する。
		// ロックオンが無効な場合のみ
		if (PlayerCamera* pc = player_->GetPlayerCamera()) {
			if(!player_->GetPlayerCamera()->IsLockOn()) pc->OnAttackHit(hitPos);
		}
	}
}

// ============================================================
// 衝突中の処理
// ============================================================
void PlayerSword::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

// ============================================================
// 衝突終了時の処理
// ============================================================
void PlayerSword::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

// ============================================================
// 衝突方向別の処理
// ============================================================
void PlayerSword::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

// ============================================================
// 衝突方向開始時の処理
// ============================================================
void PlayerSword::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}
// ============================================================
// VFXアセットの読み込み
// ============================================================
void PlayerSword::LoadVfxAssets(const std::string vfxAssetPath)
{
	trailEmitter_->LoadAsset(vfxAssetPath);
}

// ============================================================
// コライダー初期化
// ============================================================
void PlayerSword::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)
	);

	obbCollider_->SetMass(0.0f);
	obbCollider_->SetCollisionEnabled(false);
	obbCollider_->SetEnablePenetration(false);
}

// ============================================================
// Json設定初期化
// ============================================================
void PlayerSword::InitJson() {
	// ------------------------------------------------------------
	// メイン情報
	// ------------------------------------------------------------
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("PlayerSword", "Resources/Json/Weapon");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("PlayerSword");
	jsonManager_->Register("Translation", &wt_.translate_);
	jsonManager_->Register("Rotate", &wt_.rotate_);
	jsonManager_->Register("Scale", &wt_.scale_);
	jsonManager_->Register("Use Anchor Point", &wt_.useAnchorPoint_);
	jsonManager_->Register("AnchorPoint", &wt_.anchorPoint_);
	jsonManager_->Register("Hand Joint Name", &handJointName_);

	// ------------------------------------------------------------
	// オフセット情報
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("OffSet");
	jsonManager_->Register("Offset Position", &offsetPos_);
	jsonManager_->Register("Offset Rotation", &offsetRot_);
	jsonManager_->Register("Offset Scale", &offsetScale_);

	// ------------------------------------------------------------
	// 杖（魔法スタイル）オフセット。中央原点モデルを手のグリップに合わせる用。
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("Staff");
	jsonManager_->Register("Staff Offset Position", &staffOffsetPos_);
	jsonManager_->Register("Staff Offset Rotation", &staffOffsetRot_);
	jsonManager_->Register("Staff Offset Scale", &staffOffsetScale_);

	// ------------------------------------------------------------
	// カラー情報
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("Color");
	jsonManager_->Register("", &obj_->GetColor());

	// ------------------------------------------------------------
	// VFX情報
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("Trail");
	jsonManager_->Register("Local Root", &localRoot);
	jsonManager_->Register("Local Tip", &localTip);

	// ------------------------------------------------------------
	// コライダー情報
	// ------------------------------------------------------------
	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("PlayerSwordCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

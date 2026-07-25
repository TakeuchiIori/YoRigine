#include "DemoPlayer.h"

// App
#include "Combo/ComboTypes.h"
#include "Guard/PlayerGuard.h"

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Debugger/Logger.h>

#ifdef USE_IMGUI
#include "imgui.h" 
#endif

// ============================================================
// デストラクタ
// ============================================================
DemoPlayer::~DemoPlayer() {
	obbCollider_->~OBBCollider();
}

// ============================================================
// 初期化
// ============================================================
void DemoPlayer::Initialize(YoRigine::Camera* camera) {
	camera_ = camera;

	// ------------------------------------------------------------
	// モデル生成と初期化
	// ------------------------------------------------------------
	obj_ = std::make_unique<YoRigine::Object3d>();
	obj_->Initialize();
	obj_->SetModel("Player.gltf", true, "Idle1");
	input_ = YoRigine::Input::GetInstance();

	wt_.Initialize();
	wt_.useAnchorPoint_ = true;
	obj_->GetModel()->GetSkeleton()->SetRootParent(&wt_);

	// ------------------------------------------------------------
	// 剣と盾の初期化
	// ------------------------------------------------------------
	playerSword_ = std::make_unique<PlayerSword>();
	playerSword_->SetObject(obj_.get());
	playerSword_->Initialize(camera_);

	playerShield_ = std::make_unique<PlayerShield>();
	playerShield_->SetObject(obj_.get());
	playerShield_->Initialize(camera_);

	// ------------------------------------------------------------
	// ボーン線や当たり判定などの初期化
	// ------------------------------------------------------------
	boneLine_ = std::make_unique<YoRigine::Line>();
	boneLine_->Initialize();
	boneLine_->SetCamera(camera_);

	InitCollision();
	InitJson();
}

// ============================================================
// コライダー初期化
// ============================================================
void DemoPlayer::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kNone)
	);
}

// ============================================================
// 更新処理
// ============================================================
void DemoPlayer::Update() {
	UpdateMotionTime();

	// ------------------------------------------------------------
	// モーション再生と描画更新
	// ------------------------------------------------------------
	obj_->UpdateAnimation();
	UpdateWorldTransform();
	playerSword_->Update();
	playerShield_->Update();
	obbCollider_->Update();
}

// ============================================================
// 各種描画処理
// ============================================================
void DemoPlayer::DrawAnimation() {
	obj_->Draw(camera_, wt_);
}

void DemoPlayer::Draw() {
	playerSword_->Draw();
	playerShield_->Draw();
}

void DemoPlayer::DrawCollision() {
	playerSword_->DrawCollision();
	playerShield_->DrawCollision();
}

void DemoPlayer::DrawBone(YoRigine::Line& line) {
	obj_->DrawBone(line, wt_.GetMatWorld());
}

void DemoPlayer::DrawShadow()
{
	obj_->DrawShadow(wt_);
}

// ============================================================
// ワールド行列更新
// ============================================================
void DemoPlayer::UpdateWorldTransform() {
	wt_.UpdateMatrix();
}

// ============================================================
// モーション速度更新
// ============================================================
void DemoPlayer::UpdateMotionTime() {
	if (motionSpeed_ != preMotionSpeed_) {
		if (obj_->GetModel()) {
			obj_->GetModel()->GetMotionSystem()->SetMotionSpeed(motionSpeed_);
		}
		preMotionSpeed_ = motionSpeed_;
	}
}

// ============================================================
// 座標・回転の取得
// ============================================================
Vector3 DemoPlayer::GetWorldPosition() {
	return {
		wt_.matWorld_.m[3][0],
		wt_.matWorld_.m[3][1],
		wt_.matWorld_.m[3][2]
	};
}

Vector3 DemoPlayer::GetCameraRotation() const {
	if (camera_ && followCamera_) {
		return followCamera_->GetRotate();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

// ============================================================
// Json設定の初期化
// ============================================================
void DemoPlayer::InitJson() {
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("DemoPlayer", "Resources/Json/Objects/DemoPlayer");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("DemoPlayer");

	// ------------------------------------------------------------
	// メイン情報
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("メイン情報");
	jsonManager_->Register("位置", &wt_.translate_);
	jsonManager_->Register("回転", &wt_.rotate_);
	jsonManager_->Register("スケール", &wt_.scale_);
	jsonManager_->Register("色", &obj_->GetColor());

	// ------------------------------------------------------------
	// UV設定
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("UV関連");
	jsonManager_->Register("アンカーポイントを使用", &wt_.useAnchorPoint_);
	jsonManager_->Register("アンカーポイント", &anchorPoint_);
	jsonManager_->Register("UVスケール", &obj_->uvScale);
	jsonManager_->Register("UV回転", &obj_->uvRotate);
	jsonManager_->Register("UV移動", &obj_->uvTranslate);

	// ------------------------------------------------------------
	// ライティング設定
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("ライティング関連");
	auto* lighting = obj_->GetMaterialLighting()->GetRaw();
	jsonManager_->Register("ライティングを有効化", &lighting->enableLighting);
	jsonManager_->Register("スペキュラを有効化", &lighting->enableSpecular);
	jsonManager_->Register("環境光を有効化", &lighting->enableEnvironment);
	jsonManager_->Register("ハーフベクトルを使用", &lighting->isHalfVector);
	jsonManager_->Register("光沢度", &lighting->shininess);
	jsonManager_->Register("環境光係数", &lighting->environmentCoefficient);

	// ------------------------------------------------------------
	// モーション・コライダー設定
	// ------------------------------------------------------------
	jsonManager_->SetTreePrefix("その他");
	jsonManager_->Register("モーションの再生速度係数", &motionSpeed_);

	jsonManager_->SetTreePrefix("モーション速度");
	jsonManager_->Register("アイドル状態速度", &motionSpeed[0]);
	jsonManager_->Register("アタック状態速度", &motionSpeed[1]);
	jsonManager_->Register("ガード状態速度", &motionSpeed[2]);

	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("TitlePlayerCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

// ============================================================
// 衝突イベント処理
// ============================================================
void DemoPlayer::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
	}
}

void DemoPlayer::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
	}
}

void DemoPlayer::OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
	}
}

void DemoPlayer::OnDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
	}
}

void DemoPlayer::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

// ============================================================
// ダメージ処理
// ============================================================
void DemoPlayer::TakeDamage(int damage) {
	hp_ -= damage;
}
#include "DemoPlayer.h"

// App
#include "Combo/ComboTypes.h"
#include "Guard/PlayerGuard.h"

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Debugger/Logger.h>

#ifdef USE_IMGUI
#include "imgui.h" 
#endif // _DEBUG

/// <summary>
/// デストラクタ
/// </summary>
DemoPlayer::~DemoPlayer() {
	obbCollider_->~OBBCollider();
}

/// <summary>
/// タイトル画面用プレイヤーの初期化
/// </summary>
void DemoPlayer::Initialize(Camera* camera) {
	camera_ = camera;

	// モデル生成と初期化
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("Player.gltf", true, "Idle1");
	input_ = YoRigine::Input::GetInstance();

	// トランスフォーム初期化
	wt_.Initialize();
	wt_.useAnchorPoint_ = true;

	// スケルトンにルートを設定
	obj_->GetModel()->GetSkeleton()->SetRootParent(&wt_);

	//------------------------------------------------------------
	// 剣と盾の初期化
	//------------------------------------------------------------
	playerSword_ = std::make_unique<PlayerSword>();
	playerSword_->SetObject(obj_.get());
	playerSword_->Initialize(camera_);

	playerShield_ = std::make_unique<PlayerShield>();
	playerShield_->SetObject(obj_.get());
	playerShield_->Initialize(camera_);

	//------------------------------------------------------------
	// ボーン線や当たり判定などの初期化
	//------------------------------------------------------------
	boneLine_ = std::make_unique<Line>();
	boneLine_->Initialize();
	boneLine_->SetCamera(camera_);

	InitCollision();
	InitJson();
}

/// <summary>
/// コライダー初期化
/// </summary>
void DemoPlayer::InitCollision() {
	// OBBコライダー作成（タイトルなので衝突は無効扱い）
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kNone)
	);
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void DemoPlayer::Update() {
	// モーション速度更新
	UpdateMotionTime();

	//------------------------------------------------------------
	// モーション再生と描画更新
	//------------------------------------------------------------
	obj_->UpdateAnimation();
	UpdateWorldTransform();
	playerSword_->Update();
	playerShield_->Update();
	obbCollider_->Update();
}

/// <summary>
/// アニメーション付き描画
/// </summary>
void DemoPlayer::DrawAnimation() {
	obj_->Draw(camera_, wt_);
}

/// <summary>
/// 剣・盾描画
/// </summary>
void DemoPlayer::Draw() {
	playerSword_->Draw();
	playerShield_->Draw();
}

/// <summary>
/// コライダー描画（デバッグ用）
/// </summary>
void DemoPlayer::DrawCollision() {
	playerSword_->DrawCollision();
	playerShield_->DrawCollision();
}

/// <summary>
/// ボーン描画（デバッグ用）
/// </summary>
void DemoPlayer::DrawBone(Line& line) {
	obj_->DrawBone(line, wt_.GetMatWorld());
}

void DemoPlayer::DrawShadow()
{
	obj_->DrawShadow(wt_);
}

/// <summary>
/// ワールド行列更新
/// </summary>
void DemoPlayer::UpdateWorldTransform() {
	wt_.UpdateMatrix();
}

/// <summary>
/// モーション速度変更時の更新
/// </summary>
void DemoPlayer::UpdateMotionTime() {
	if (motionSpeed_ != preMotionSpeed_) {
		if (obj_->GetModel()) {
			obj_->GetModel()->GetMotionSystem()->SetMotionSpeed(motionSpeed_);
		}
		preMotionSpeed_ = motionSpeed_;
	}
}

/// <summary>
/// ワールド座標を取得
/// </summary>
Vector3 DemoPlayer::GetWorldPosition() {
	return {
		wt_.matWorld_.m[3][0],
		wt_.matWorld_.m[3][1],
		wt_.matWorld_.m[3][2]
	};
}

/// <summary>
/// 現在のカメラ回転を取得
/// </summary>
Vector3 DemoPlayer::GetCameraRotation() const {
	if (camera_ && followCamera_) {
		return followCamera_->GetRotate();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// Json設定の初期化
/// </summary>
void DemoPlayer::InitJson() {
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("DemoPlayer", "Resources/Json/Objects/DemoPlayer");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("DemoPlayer");

	//------------------------------------------------------------
	// メイン情報
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("メイン情報");
	jsonManager_->Register("位置", &wt_.translate_);
	jsonManager_->Register("回転", &wt_.rotate_);
	jsonManager_->Register("スケール", &wt_.scale_);
	jsonManager_->Register("色", &obj_->GetColor());

	//------------------------------------------------------------
	// UV設定
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("UV関連");
	jsonManager_->Register("アンカーポイントを使用", &wt_.useAnchorPoint_);
	jsonManager_->Register("アンカーポイント", &anchorPoint_);
	jsonManager_->Register("UVスケール", &obj_->uvScale);
	jsonManager_->Register("UV回転", &obj_->uvRotate);
	jsonManager_->Register("UV移動", &obj_->uvTranslate);

	//------------------------------------------------------------
	// ライティング設定
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("ライティング関連");
	auto* lighting = obj_->GetMaterialLighting()->GetRaw();
	jsonManager_->Register("ライティングを有効化", &lighting->enableLighting);
	jsonManager_->Register("スペキュラを有効化", &lighting->enableSpecular);
	jsonManager_->Register("環境光を有効化", &lighting->enableEnvironment);
	jsonManager_->Register("ハーフベクトルを使用", &lighting->isHalfVector);
	jsonManager_->Register("光沢度", &lighting->shininess);
	jsonManager_->Register("環境光係数", &lighting->environmentCoefficient);

	//------------------------------------------------------------
	// モーション設定
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("その他");
	jsonManager_->Register("モーションの再生速度係数", &motionSpeed_);

	jsonManager_->SetTreePrefix("モーション速度");
	jsonManager_->Register("アイドル状態速度", &motionSpeed[0]);
	jsonManager_->Register("アタック状態速度", &motionSpeed[1]);
	jsonManager_->Register("ガード状態速度", &motionSpeed[2]);

	//------------------------------------------------------------
	// コライダー設定
	//------------------------------------------------------------
	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("TitlePlayerCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

/// <summary>
/// 衝突開始時の処理
/// </summary>
void DemoPlayer::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
		// TODO: 必要に応じてタイトル演出を追加
	}
}

/// <summary>
/// 衝突中の処理
/// </summary>
void DemoPlayer::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
		// TODO: エフェクトまたはカラー変更など
	}
}

/// <summary>
/// 衝突終了時の処理
/// </summary>
void DemoPlayer::OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
		// TODO: 終了時のエフェクト
	}
}

/// <summary>
/// 衝突方向別処理
/// </summary>
void DemoPlayer::OnDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) {
		// TODO: 当たった方向に応じた演出を追加
	}
}

void DemoPlayer::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

/// <summary>
/// ダメージ処理
/// </summary>
void DemoPlayer::TakeDamage(int damage) {
	hp_ -= damage;
}

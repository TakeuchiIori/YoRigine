#include "FieldEnemy.h"
#include "Player/Player.h"
#include "FieldEnemyManager.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Systems/GameTime/GameTime.h"
#include <Debugger/Logger.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include <Collision/Core/CollisionManager.h>

// States
#include "States/FieldEnemyPatrolState.h"
#include "States/FieldEnemyChaseState.h"
#include <Collision/AreaCollision/Base/AreaManager.h>

/// <summary>
/// コンストラクタ
/// </summary>
FieldEnemy::FieldEnemy() = default;

/// <summary>
/// デストラクタ（OBBコライダー破棄）
/// </summary>
FieldEnemy::~FieldEnemy() {
	if (obbCollider_) {
		obbCollider_->~OBBCollider();
	}
}

/// <summary>
/// 敵の初期化処理（Object3d・WorldTransform・Collision）
/// </summary>
/// <param name="camera">描画に使用するカメラ</param>
void FieldEnemy::Initialize(Camera* camera) {
	camera_ = camera;

	// Object3dの初期化
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();

	// WorldTransformの初期化
	wt_.Initialize();

	// 当たり判定の初期化
	InitCollision();
	// Json初期化
	InitJson();

}

/// <summary>
/// JSONから読み込まれたフィールド敵データで初期化
/// </summary>
/// <param name="data">フィールド敵のデータ構造体</param>
/// <param name="spawnPosition">出現位置</param>
void FieldEnemy::InitializeFieldData(const FieldEnemyData& data, const Vector3& spawnPosition) {
	enemyData_ = data;
	spawnPosition_ = spawnPosition;
	wt_.translate_ = spawnPosition;
	wt_.scale_ = data.scale;

	// モデル設定
	if (obj_) {
		obj_->SetModel(data.modelPath);

		// カスタムカラー適用
		if (data.useCustomColor) {
			obj_->SetMaterialColor(data.modelColor);
		}
	}

	// 初期状態をPatrolに設定
	ChangeState(std::make_unique<FieldEnemyPatrolState>());

	std::string battleInfo = data.battleEnemyIds.empty() ?
		data.battleEnemyId :
		std::to_string(data.battleEnemyIds.size()) + "体グループ";

	Logger("[FieldEnemy] JSONから初期化: ID=" + data.enemyId +
		", バトル: " + battleInfo +
		", タイプ: " + data.GetBattleTypeString() + "\n");
}

/// <summary>
/// 当たり判定（OBBコライダー）の初期化
/// </summary>
void FieldEnemy::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this,
		&wt_,
		camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kFieldEnemy)
	);
}

/// <summary>
/// JsonManagerの初期化
/// </summary>
void FieldEnemy::InitJson() {
	std::string identifier = enemyData_.enemyId;
	if (identifier.empty()) identifier = "UnknownEnemy";

	jsonManager_ = std::make_unique<YoRigine::JsonManager>(identifier, "Resources/Json/Objects/FieldEnemies");
	jsonManager_->SetCategory(identifier);
	obbCollider_->InitJson(jsonManager_.get());
}

/// <summary>
/// フィールド敵の毎フレーム更新処理
/// </summary>
void FieldEnemy::Update() {
	if (logicalState_ == FieldEnemyState::Despawn) {
		return;
	}

	float dt = YoRigine::GameTime::GetDeltaTime();
	stateTimer_ += dt;

	// エンカウントクールダウン更新
	UpdateEncounterCooldown(dt);

	// 現在のStateを更新
	if (currentState_) {
		currentState_->Update(*this, dt);
	}

	// エリアマネージャーによる位置制限更新
	AreaManager::GetInstance()->UpdateSingleObject(&wt_);
	// 基底クラスの更新処理
	wt_.UpdateMatrix();
	obbCollider_->Update();
}

/// <summary>
/// ステートを切り替える
/// </summary>
/// <param name="newState">切り替え先のステート</param>
void FieldEnemy::ChangeState(std::unique_ptr<IEnemyState<FieldEnemy>> newState) {
	if (currentState_) {
		currentState_->Exit(*this);
	}

	currentState_ = std::move(newState);

	if (currentState_) {
		currentState_->Enter(*this);
	}

	stateTimer_ = 0.0f;
}

void FieldEnemy::ApplyUpdatedData(const FieldEnemyData& data)
{
	enemyData_ = data;
	wt_.scale_ = data.scale;

	if (obj_) {
		// モデルパスが変わった場合は再ロードが必要ならここで処理（通常はスケールや色のみで十分なはず）
		if (data.useCustomColor) {
			obj_->SetMaterialColor(data.modelColor);
		} else {
			obj_->SetMaterialColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
	}
}

/// <summary>
/// プレイヤーのワールド座標を取得
/// </summary>
/// <returns>プレイヤー座標。存在しない場合は(0,0,0)</returns>
Vector3 FieldEnemy::GetPlayerPosition() const {
	if (player_) {
		return player_->GetWorldPosition();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// エンカウントクールダウンの更新処理
/// </summary>
/// <param name="dt">経過時間（デルタタイム）</param>
void FieldEnemy::UpdateEncounterCooldown(float dt) {
	if (encounterCooldown_ > 0.0f) {
		encounterCooldown_ -= dt;

		// クールダウン終了時にログ出力
		if (encounterCooldown_ <= 0.0f) {
			Logger("[FieldEnemy] エンカウントクールダウン終了: " + enemyData_.enemyId + "\n");
		}
	}
}

/// <summary>
/// 他コライダーと接触した瞬間の処理
/// </summary>
void FieldEnemy::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	uint32_t typeID = other->GetTypeID();

	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
		if (CanTriggerEncounter()) {
			TriggerEncounter();
		}
	}
}

/// <summary>
/// 他コライダーとの接触中処理
/// </summary>
void FieldEnemy::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	uint32_t typeID = other->GetTypeID();

	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
		if (CanTriggerEncounter()) {
			//TriggerEncounter();
		}
	}
}

/// <summary>
/// 他コライダーとの離脱処理
/// </summary>
void FieldEnemy::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {
	// 必要に応じて実装
}

/// <summary>
/// 方向付き衝突処理（接触方向に応じたコールバック）
/// </summary>
void FieldEnemy::OnDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir) {
	// 1. 相手がプレイヤーの武器かチェック
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {

		// 3. 背後(Back)からの攻撃のみに絞る
		if (dir == HitDirection::Back) {
			if (CanTriggerEncounter()) {
				Logger("[FieldEnemy] GetSelfLocalHitDirectionによる背後判定成功\n");

				TriggerEncounter();

				if (player_ && player_->GetCombat()) {
					TakeDamage(player_->GetCombat()->GetCombo()->GetCurrentDamage());
				}
			}
		}
	}
}

void FieldEnemy::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {

}

/// <summary>
/// バトルへのエンカウントを発生させる
/// </summary>
void FieldEnemy::TriggerEncounter() {
	if (fieldEnemyManager_ && !hasTriggeredEncounter_) {
		hasTriggeredEncounter_ = true;
		encounterCooldown_ = encounterCooldownDuration_;

		auto battleIds = GetBattleEnemyIds();
		std::string battleInfo = (battleIds.size() > 1) ?
			std::to_string(battleIds.size()) + "体バトル" :
			"単体バトル";

		Logger("[FieldEnemy] エンカウント発生: " + enemyData_.enemyId +
			" (" + battleInfo + ") クールダウン: " +
			std::to_string(encounterCooldownDuration_) + "秒\n");

		// マネージャーに通知
		fieldEnemyManager_->OnEnemyEncounter(this);
	}
}

void FieldEnemy::TakeDamage(const float damage)
{
	takeDamage_ = damage;
}

/// <summary>
/// エンカウント状態をリセットする
/// </summary>
void FieldEnemy::ResetEncounterState() {
	hasTriggeredEncounter_ = false;
	encounterCooldown_ = 0.0f;

	Logger("[FieldEnemy] エンカウント状態をリセット: " + enemyData_.enemyId + "\n");
}

/// <summary>
/// 敵モデルの描画処理
/// </summary>
void FieldEnemy::Draw() {
	if (logicalState_ != FieldEnemyState::Despawn) {
		if (obj_) {
			obj_->Draw(camera_, wt_);
		}
	}
}

void FieldEnemy::DrawShadow()
{
	if (logicalState_ != FieldEnemyState::Despawn) {
		if (obj_) {
			obj_->DrawShadow(wt_);
		}
	}
}

/// <summary>
/// 当たり判定のデバッグ描画
/// </summary>
void FieldEnemy::DrawCollision() {
	if (obbCollider_) {
		obbCollider_->Draw();
	}
}

void FieldEnemy::DrawLine(Line* line) {
	if(!line || logicalState_ == FieldEnemyState::Despawn) {
		return;
	}
	line->DrawCone(
		GetPosition(),
		GetRotae().y,
		enemyData_.viewDistance,
		enemyData_.viewAngle
	);
}

#include "PlayerCombo.h"
#include "../Player.h"
#include "AttackDatabase.h"
#include "AttackEditor.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

// グローバルエディターインスタンス（1つだけ存在）
static std::unique_ptr<AttackDataEditor> g_AttackEditor = nullptr;

/// <summary>
/// コンストラクタ：プレイヤーのコンボシステムを初期化する
/// </summary>
/// <param name="owner">このコンボシステムを所有するプレイヤー</param>
PlayerCombo::PlayerCombo(Player* owner)
	: owner_(owner), currentCC_(ccConfig_.maxCC) {
	//---------------------------------------------------------------------------------------------
	// 攻撃データベースの初期化（A術 / B術 / 奥義）
	//---------------------------------------------------------------------------------------------
	InitializeAttacks();

	//---------------------------------------------------------------------------------------------
	// エディターの初期化（初回のみ）
	//---------------------------------------------------------------------------------------------
	if (!g_AttackEditor) {
		g_AttackEditor = std::make_unique<AttackDataEditor>();
		g_AttackEditor->SetFilePath("Resources/Json/Combo/AttackData.json");
		g_AttackEditor->SetReloadCallback([this]() { ReloadAttacks(); });               // ★
		g_AttackEditor->SetOpen(false);
	}
}

/// <summary>
/// 毎フレーム更新処理
/// コンボ状態・CC回復・タイマー進行を制御する
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerCombo::Update(float deltaTime) {
	// 前フレームの状態を保存
	previousState_ = currentState_;

	//---------------------------------------------------------------------------------------------
	// 各種タイマー更新（CC回復・コンボ時間・状態経過）
	//---------------------------------------------------------------------------------------------
	UpdateCC(deltaTime);
	UpdateComboTimer(deltaTime);
	stateTimer_ += deltaTime;

	//---------------------------------------------------------------------------------------------
	// 現在の状態ごとの更新処理
	//---------------------------------------------------------------------------------------------
	switch (currentState_) {

	case ComboState::Attacking:
		// 攻撃実行中
		UpdateAttacking();
		break;

	case ComboState::CanContinue:
		// コンボ継続受付中
		UpdateCanContinue();
		break;

	case ComboState::Recovery:
		// 攻撃硬直中
		UpdateRecovery();
		break;

	case ComboState::Idle:
		// コンボ中に一定時間入力がなければ自動的にリセット
		if (GetComboCount() > 0 && comboTimer_ >= config_.comboResetTime) {
			ResetCombo();
		}
		break;

	case ComboState::Finished:
		// コンボ終了 → 待機状態に戻す
		ChangeState(ComboState::Idle);

		// コールバックが登録されていれば呼び出す
		if (onComboEnd_) {
			onComboEnd_(GetComboCount());
		}
		break;
	}
}

/// <summary>
/// 攻撃を試みる処理
/// 攻撃可能なら対応する攻撃データを取得して実行する
/// </summary>
/// <param name="attackType">攻撃タイプ（A術 / B術 / 奥義）</param>
/// <returns>攻撃が成功した場合 true</returns>
bool PlayerCombo::TryAttack(AttackType attackType) {
	// 攻撃が可能かチェック
	if (!CanAttack(attackType)) return false;

	//---------------------------------------------------------------------------------------------
	// 攻撃データを攻撃タイプに応じて検索
	//---------------------------------------------------------------------------------------------
	AttackData* attack = FindBestAttack(attackType);
	if (!attack) return false;

	//---------------------------------------------------------------------------------------------
	// CC（チェインキャパシティ）の残量を確認
	//---------------------------------------------------------------------------------------------
	if (!HasSufficientCC(attack->ccCost)) return false;

	//---------------------------------------------------------------------------------------------
	// 攻撃実行
	//---------------------------------------------------------------------------------------------
	ExecuteAttack(*attack);
	return true;
}

/// <summary>
/// 攻撃可能か判定する
/// 現在のコンボ状態に応じて攻撃を受け付けるか決定
/// </summary>
bool PlayerCombo::CanAttack([[maybe_unused]] AttackType attackType) const {
	switch (currentState_) {
	case ComboState::Idle:        // 待機中は常に攻撃可能
	case ComboState::CanContinue: // コンボ継続中も攻撃可能
		return true;

	case ComboState::Attacking:   // 攻撃中はキャンセル可能な場合のみ
		return currentAttack_ && currentAttack_->canCancel;

	default:
		return false;
	}
}

/// <summary>
/// 現在の状況に応じて最適な攻撃データを検索
/// 同タイプ連続なら派生攻撃を選択、それ以外は基本攻撃
/// </summary>
AttackData* PlayerCombo::FindBestAttack(AttackType type) {
	auto it = attackDatabase_.find(type);
	if (it == attackDatabase_.end() || it->second.empty()) return nullptr;

	const auto& attacks = it->second;

	//---------------------------------------------------------------------------------------------
	// コンボ初撃 or 連撃中の判定
	//---------------------------------------------------------------------------------------------
	if (GetComboCount() == 0) {
		// 初撃なら最初の攻撃
		return const_cast<AttackData*>(&attacks[0]);
	} else {
		AttackType lastType = comboChain_.back().type;

		if (lastType == type) {
			// 同タイプ連続攻撃時：同タイプの連続数に応じて派生攻撃を選択
			int sameTypeCount = 0;
			for (auto combo = comboChain_.rbegin(); combo != comboChain_.rend(); ++combo) {
				if (combo->type == type) sameTypeCount++;
				else break;
			}
			int index = sameTypeCount % static_cast<int>(attacks.size());
			return const_cast<AttackData*>(&attacks[index]);
		} else {
			// 異なるタイプからの派生：基本攻撃に戻す
			return const_cast<AttackData*>(&attacks[0]);
		}
	}
}

/// <summary>
/// 攻撃実行処理
/// 実際に攻撃データを適用し、コンボ状態を更新する
/// </summary>
void PlayerCombo::ExecuteAttack(const AttackData& attack) {
	//---------------------------------------------------------------------------------------------
	// 攻撃データの適用と状態更新
	//---------------------------------------------------------------------------------------------
	currentAttack_ = const_cast<AttackData*>(&attack);
	comboChain_.push_back(attack);
	comboDamageMultiplier_ = CalculateDamageMultiplier();
	ChangeState(ComboState::Attacking);
	comboTimer_ = 0.0f;



	//---------------------------------------------------------------------------------------------
	// 攻撃開始／継続時のコールバック通知
	//---------------------------------------------------------------------------------------------
	if (GetComboCount() == 1) {
		if (onAttackStart_) onAttackStart_(attack);
	} else {
		if (onAttackContinue_) onAttackContinue_(attack);
	}
}

/// <summary>
/// 攻撃状態の更新処理
/// 攻撃時間経過後、次のコンボ状態へ遷移
/// </summary>
void PlayerCombo::UpdateAttacking() {
	// 現在の攻撃が無効なら待機状態に戻す
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	// 経過時間をフレームに変換
	const float frameDuration = (currentAttack_->fps > 0)
		? 1.0f / static_cast<float>(currentAttack_->fps)
		: 1.0f / 60.0f;

	const int currentFrame = static_cast<int>(stateTimer_ / frameDuration);

	// hitStart〜hitEnd の間だけ剣コライダーをON
	const bool inHitWindow = (currentAttack_->hitEnd > currentAttack_->hitStart)
		&& (currentFrame >= currentAttack_->hitStart)
		&& (currentFrame < currentAttack_->hitEnd);


	if (onSwordColliderChanged_) onSwordColliderChanged_(inHitWindow);
	// 攻撃時間終了チェック
	if (stateTimer_ >= currentAttack_->duration) {
		if (currentAttack_->canChainToAny && currentCC_ > 0)
			ChangeState(ComboState::CanContinue); // 追加入力受付へ
		else
			ChangeState(ComboState::Recovery);    // 攻撃硬直へ
	}
}

/// <summary>
/// コンボ継続可能状態の更新
/// 一定時間入力がなければリカバリー状態へ
/// </summary>
void PlayerCombo::UpdateCanContinue() {
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	// コンボ継続受付時間が過ぎたらリカバリーへ移行
	if (stateTimer_ >= currentAttack_->continueWindow)
		ChangeState(ComboState::Recovery);
}

/// <summary>
/// 攻撃後の硬直状態更新
/// 硬直時間終了後はコンボ終了へ遷移
/// </summary>
void PlayerCombo::UpdateRecovery() {
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	// 攻撃後硬直の経過チェック
	if (stateTimer_ >= currentAttack_->recovery)
		ChangeState(ComboState::Finished);
}

/// <summary>
/// CC（チェインキャパシティ）の回復処理
/// 待機中のみCCが時間経過で回復する
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerCombo::UpdateCC(float deltaTime) {
	// 現在の状態に応じてCC回復を制御
	switch (currentState_) {
	case ComboState::Idle:
		// Idle状態のみCC回復を開始
		ccRegenTimer_ += deltaTime;
		if (ccRegenTimer_ >= ccConfig_.regenDelay) {
			RecoverCC(static_cast<int>(ccConfig_.regenRate * deltaTime));
		}
		break;

	default:
		// 攻撃中はCC回復を停止
		ccRegenTimer_ = 0.0f;
		break;
	}
}

/// <summary>
/// コンボタイマーの更新処理
/// コンボ継続の受付やリセットタイミングを管理
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerCombo::UpdateComboTimer(float deltaTime) {
	comboTimer_ += deltaTime;
}

/// <summary>
/// コンボ状態を変更する
/// 状態遷移時にEnter / Exit処理を適用
/// </summary>
/// <param name="newState">新しい状態</param>
void PlayerCombo::ChangeState(ComboState newState) {
	if (currentState_ == newState) return;

	ExitState(currentState_);
	currentState_ = newState;
	EnterState(newState);
}

/// <summary>
/// 状態に入った時の処理
/// 攻撃開始時などに必要な初期化を行う
/// </summary>
/// <param name="newState">新しい状態</param>
void PlayerCombo::EnterState(ComboState newState) {
	stateTimer_ = 0.0f;

	switch (newState) {
	case ComboState::Attacking:
		// 攻撃開始時はCC回復を一時停止
		ccRegenTimer_ = 0.0f;
		break;

	default:
		break;
	}
}

/// <summary>
/// 状態を抜ける時の処理
/// 状態終了時に必要なクリーンアップを行う（現状は空）
/// </summary>
/// <param name="oldState">以前の状態</param>
void PlayerCombo::ExitState(ComboState oldState) {
	// 必要に応じて終了処理
	switch (oldState) {
	case ComboState::Attacking:
		// 攻撃終了時はヒットウィンドウを閉じる
		if (onSwordColliderChanged_) onSwordColliderChanged_(false);
		break;
	case ComboState::CanContinue:
		break;
	case ComboState::Recovery:
		break;
	case ComboState::Idle:
		break;
	case ComboState::Finished:
		break;
	}

}

/// <summary>
/// コンボのダメージ倍率を計算する
/// コンボ数やチェーン構成に応じてダメージ倍率を上昇させる
/// </summary>
/// <returns>計算されたダメージ倍率</returns>
float PlayerCombo::CalculateDamageMultiplier() const {
	if (GetComboCount() <= 1) return 1.0f; // 初撃は等倍

	float multiplier = 1.0f;

	//---------------------------------------------------------------------------------------------
	// コンボ数に応じた基本倍率上昇（1コンボごとに+10%）
	//---------------------------------------------------------------------------------------------
	multiplier += (GetComboCount() - 1) * 0.1f;

	//---------------------------------------------------------------------------------------------
	// 推奨チェーンボーナス（A→BやB→Aなど）
	//---------------------------------------------------------------------------------------------
	if (GetComboCount() >= 2) {
		const AttackData& prev = comboChain_[comboChain_.size() - 2];
		const AttackData& curr = comboChain_[comboChain_.size() - 1];

		if (IsChainPreferred(prev.type, curr.type)) {
			multiplier *= config_.chainBonus;
		}
	}

	//---------------------------------------------------------------------------------------------
	// コンボ減衰（3段以降は徐々に火力が下がる）
	//---------------------------------------------------------------------------------------------
	for (int i = 3; i < GetComboCount(); ++i) {
		multiplier *= config_.damageDecay;
	}

	return multiplier;
}

/// <summary>
/// 攻撃タイプ間のチェーンが推奨パターンかどうかを判定
/// 例：A→B, B→A, A→奥義 などはボーナスが付く
/// </summary>
/// <param name="from">前の攻撃タイプ</param>
/// <param name="to">次の攻撃タイプ</param>
/// <returns>推奨チェーンであれば true</returns>
bool PlayerCombo::IsChainPreferred(AttackType from, AttackType to) const {
	if (from == AttackType::A_Arte && to == AttackType::B_Arte) return true;
	if (from == AttackType::B_Arte && to == AttackType::A_Arte) return true;
	return false;
}


/// <summary>
/// CCを消費する処理
/// 攻撃やスキル実行時にCCを減らし、変化を通知する
/// </summary>
/// <param name="amount">消費量</param>
void PlayerCombo::ConsumeCC(int amount) {
	int oldCC = currentCC_;
	currentCC_ = std::max(0, currentCC_ - amount);

	// CCが変化した場合のみイベント通知
	if (onCCChanged_) {
		onCCChanged_(oldCC, currentCC_);
	}
}

/// <summary>
/// CCを回復させる処理
/// 回避やカウンター成功時などで回復する
/// </summary>
/// <param name="amount">回復量</param>
void PlayerCombo::RecoverCC(int amount) {
	int oldCC = currentCC_;
	currentCC_ = std::min(ccConfig_.maxCC, currentCC_ + amount);

	// CCが変化した場合のみイベント通知
	if (onCCChanged_) {
		onCCChanged_(oldCC, currentCC_);
	}
}

/// <summary>
/// 回避成功時のCC回復処理
/// </summary>
void PlayerCombo::OnDodgeSuccess() {
	RecoverCC(ccConfig_.dodgeRecovery);
}

/// <summary>
/// カウンター成功時のCC回復処理
/// </summary>
void PlayerCombo::OnCounterHit() {
	RecoverCC(ccConfig_.counterRecovery);
}

/// <summary>
/// 攻撃がヒットした瞬間に呼ばれる
/// 敵の位置に向かって補間ステップを開始する
/// </summary>
/// <param name="enemyPosition">ヒットした敵の世界座標</param>
void PlayerCombo::OnHitStep(const Vector3& enemyPosition) {
	if (!currentAttack_) return;
	if (!owner_ || !owner_->GetMovement()) return;

	// currentAttack_ の stepDistance が 0 なら何もしない
	if (currentAttack_->stepDistance <= 0.0f) return;

	// PlayerMovement に敵の位置と前進距離を渡して補間を開始
	owner_->GetMovement()->RequestAttackStep(enemyPosition, currentAttack_->stepDistance);
}

/// <summary>
/// コンボを完全にリセット
/// チェーン履歴や現在攻撃を全て破棄し、待機状態に戻す
/// </summary>
void PlayerCombo::ResetCombo() {
	//---------------------------------------------------------------------------------------------
	// 現在の攻撃・コンボ情報をクリア
	//---------------------------------------------------------------------------------------------
	comboChain_.clear();
	currentAttack_ = nullptr;
	comboDamageMultiplier_ = 1.0f;
	comboTimer_ = 0.0f;

	// 状態リセット
	if (currentState_ != ComboState::Idle) {
		ChangeState(ComboState::Idle);
	}


	// コールバック通知
	if (onComboReset_) {
		onComboReset_();
	}
}

/// <summary>
/// コンボをキャンセル
/// 現在の攻撃がキャンセル可能ならリセットを実行
/// </summary>
void PlayerCombo::CancelCombo() {
	if (currentAttack_ && currentAttack_->canCancel) {
		ResetCombo();
	}
}

/// <summary>
/// 強制的にコンボを終了する
/// 通常ルートを飛ばして直接「終了状態」へ遷移
/// </summary>
void PlayerCombo::ForceEndCombo() {

	ChangeState(ComboState::Finished);
}


/// <summary>
/// 現在のコンボダメージ倍率を取得
/// </summary>
/// <returns>現在のコンボに適用されるダメージ倍率</returns>
float PlayerCombo::GetComboDamageMultiplier() const {
	return comboDamageMultiplier_;
}

/// <summary>
/// 現在のコンボ状態を文字列として返す
/// デバッグ表示やログ出力用
/// </summary>
/// <param name="state">対象のコンボ状態</param>
/// <returns>状態名文字列</returns>
const char* PlayerCombo::GetStateString(ComboState state) const {
	switch (state) {
	case ComboState::Idle:        return "Idle";
	case ComboState::Attacking:   return "Attacking";
	case ComboState::CanContinue: return "CanContinue";
	case ComboState::Recovery:    return "Recovery";
	case ComboState::Finished:    return "Finished";
	default:                      return "Unknown";
	}
}

/// <summary>
/// 攻撃データベース初期化処理
/// JSONファイルから攻撃データを読み込み、タイプ別に分類する
/// </summary>
void PlayerCombo::InitializeAttacks()
{
	// JSON ファイルパス
	const std::string path = "Resources/Json/Combo/AttackData.json";

	// AttackDatabase（グローバル）からロード
	if (!AttackDatabase::LoadFromFile(path))
	{
		OutputDebugStringA("[PlayerCombo] AttackData.json load failed!\n");
		return;
	}

	// 現行の attackDatabase_ を初期化
	attackDatabase_.clear();

	auto& list = AttackDatabase::Get();

	// AttackType ごとに仕分け
	for (auto& atk : list)
	{
		attackDatabase_[atk.type].push_back(atk);
	}

	// 状態の初期化
	currentAttack_ = nullptr;
	comboChain_.clear();
	currentState_ = ComboState::Idle;
	previousState_ = ComboState::Idle;

	// 初期状態のステータスやタイマー
	stateTimer_ = 0.0f;
	comboTimer_ = 0.0f;
	currentCC_ = ccConfig_.maxCC;
	OutputDebugStringA("[PlayerCombo] AttackData loaded and grouped by AttackType.\n");
}

/// <summary>
/// 攻撃データをリロード（エディター保存後に呼ぶ）
/// </summary>
void PlayerCombo::ReloadAttacks() {
	InitializeAttacks();
	OutputDebugStringA("[PlayerCombo] Attack data reloaded!\n");
}

/// <summary>
/// デバッグ表示（ImGui）
/// コンボ状態・CC情報・攻撃データなどをリアルタイムで可視化する
/// </summary>
void PlayerCombo::ShowDebugImGui() {
#ifdef USE_IMGUI
	//---------------------------------------------------------------------------------------------
	// エディターボタン
	//---------------------------------------------------------------------------------------------

	// ★ エディターウィンドウ描画（isOpen_ が true のときだけ表示）
	if (g_AttackEditor && g_AttackEditor->IsOpen())
	{
		ImGui::Begin("コンボエディター", nullptr);
		g_AttackEditor->DrawImGui();
		ImGui::End();
	}

	if (ImGui::Button("攻撃エディターを開く")) {
		if (g_AttackEditor) {
			g_AttackEditor->SetOpen(true);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("攻撃データをリロード")) {
		ReloadAttacks();
	}

	ImGui::Separator();

	//---------------------------------------------------------------------------------------------
	// 現在の状態情報表示
	//---------------------------------------------------------------------------------------------
	ImGui::Text("=== コンボ状態 ===");
	ImGui::Text("現在の状態: %s", GetStateString(currentState_));
	ImGui::Text("前の状態: %s", GetStateString(previousState_));

	// 状態変化を視覚的に強調
	if (StateChanged()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
		ImGui::Text("★ 状態が変化しました！");
		ImGui::PopStyleColor();
	}

	//---------------------------------------------------------------------------------------------
	// CC関連情報表示
	//---------------------------------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== CCシステム ===");
	ImGui::Text("CC: %d / %d", currentCC_, ccConfig_.maxCC);
	ImGui::ProgressBar(static_cast<float>(currentCC_) / ccConfig_.maxCC, ImVec2(0.0f, 0.0f), "");
	ImGui::Text("CC回復タイマー: %.2f 秒", ccRegenTimer_);

	//---------------------------------------------------------------------------------------------
	// コンボ情報表示
	//---------------------------------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== コンボ情報 ===");
	ImGui::Text("コンボ数: %d / %d", GetComboCount(), config_.maxLength);
	ImGui::Text("ダメージ倍率: x%.2f", GetComboDamageMultiplier());
	ImGui::Text("コンボタイマー: %.2f 秒", comboTimer_);
	ImGui::Text("状態タイマー: %.2f 秒", stateTimer_);

	//---------------------------------------------------------------------------------------------
	// 攻撃データベース情報
	//---------------------------------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== 攻撃データベース ===");
	ImGui::Text("A攻撃数: %d", static_cast<int>(attackDatabase_[AttackType::A_Arte].size()));
	ImGui::Text("B攻撃数: %d", static_cast<int>(attackDatabase_[AttackType::B_Arte].size()));

	//---------------------------------------------------------------------------------------------
	// 現在攻撃情報
	//---------------------------------------------------------------------------------------------
	if (currentAttack_) {
		ImGui::Separator();
		ImGui::Text("=== 現在の攻撃 ===");
		ImGui::Text("攻撃名: %s", currentAttack_->name.c_str());
		ImGui::Text("アニメーション: %s", currentAttack_->animationName.c_str());
		ImGui::Text("ダメージ: %.1f", currentAttack_->baseDamage);
		ImGui::Text("CC消費: %d", currentAttack_->ccCost);
		ImGui::Text("キャンセル可能: %s", currentAttack_->canCancel ? "はい" : "いいえ");
		ImGui::Text("自由チェーン: %s", currentAttack_->canChainToAny ? "はい" : "いいえ");

		// 攻撃タイプの表示
		const char* typeStr = "";
		switch (currentAttack_->type) {
		case AttackType::A_Arte: typeStr = "A"; break;
		case AttackType::B_Arte: typeStr = "B"; break;
		}
		ImGui::Text("タイプ: %s", typeStr);
	}

	//---------------------------------------------------------------------------------------------
	// コンボ履歴（チェーン）表示
	//---------------------------------------------------------------------------------------------
	if (!comboChain_.empty()) {
		ImGui::Separator();
		ImGui::Text("=== コンボチェーン履歴 ===");

		for (size_t i = 0; i < comboChain_.size(); ++i) {
			const AttackData& attack = comboChain_[i];
			bool isCurrent = (currentAttack_ == &attack);

			if (isCurrent) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			}

			ImGui::Text("[%zu] %s", i + 1, attack.name.c_str());

			if (isCurrent) {
				ImGui::PopStyleColor();
			}

			// 攻撃データツールチップ表示
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("ダメージ: %.1f", attack.baseDamage);
				ImGui::Text("CC消費: %d", attack.ccCost);
				ImGui::Text("持続時間: %.2f秒", attack.duration);
				ImGui::EndTooltip();
			}
		}
	}

	//---------------------------------------------------------------------------------------------
	// 操作用ボタン群
	//---------------------------------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== 操作テスト ===");

	if (ImGui::Button("コンボリセット")) ResetCombo();
	ImGui::SameLine();
	if (ImGui::Button("強制終了")) ForceEndCombo();
	ImGui::SameLine();
	if (ImGui::Button("CC全回復")) currentCC_ = ccConfig_.maxCC;

	// 攻撃テスト
	ImGui::Separator();
	if (ImGui::Button("A攻撃")) TryAttack(AttackType::A_Arte);
	ImGui::SameLine();
	if (ImGui::Button("B攻撃")) TryAttack(AttackType::B_Arte);
#endif
}

/// <summary>
/// YoRigine::JsonManager 初期化設定
/// CC・コンボ・攻撃データのパラメータをGUI/ファイル編集可能にする
/// </summary>
/// <param name="jsonManager">設定対象のYoRigine::JsonManager</param>
void PlayerCombo::InitJson(YoRigine::JsonManager* jsonManager) {
	//---------------------------------------------------------------------------------------------
	// CC関連設定項目
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("CCの設定");
	jsonManager->Register("最大CCの値", &ccConfig_.maxCC);
	jsonManager->Register("CC回復速度（秒）", &ccConfig_.regenRate);
	jsonManager->Register("攻撃後のCC回復開始遅延", &ccConfig_.regenDelay);
	jsonManager->Register("回避成功時のCC回復量", &ccConfig_.dodgeRecovery);
	jsonManager->Register("カウンター成功時のCC回復量", &ccConfig_.counterRecovery);

	//---------------------------------------------------------------------------------------------
	// コンボ挙動設定項目
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("コンボの設定");
	jsonManager->Register("最大コンボ長", &config_.maxLength);
	jsonManager->Register("ダメージ減衰率", &config_.damageDecay);
	jsonManager->Register("チェーンボーナス倍率", &config_.chainBonus);
	jsonManager->Register("自由チェーン有効", &config_.enableFreeChain);
	jsonManager->Register("コンボリセット時間（秒）", &config_.comboResetTime);
}
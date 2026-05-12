#include "PlayerCombo.h"
#include "../Player.h"
#include "AttackDatabase.h"
#include "AttackEditor.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

static std::unique_ptr<AttackDataEditor> g_AttackEditor = nullptr;

// ============================================================
// コンストラクタ処理
// コンボシステム全体の初期化および攻撃エディターのセットアップを行う
// ============================================================
PlayerCombo::PlayerCombo(Player* owner)
	: owner_(owner), currentCC_(ccConfig_.maxCC) {

	// ------------------------------------------------------------
	// 攻撃データの読み込みと分類
	// ------------------------------------------------------------
	InitializeAttacks();

	// ------------------------------------------------------------
	// インゲームエディターの生成とコールバック登録（初回のみ）
	// ------------------------------------------------------------
	if (!g_AttackEditor) {
		g_AttackEditor = std::make_unique<AttackDataEditor>();
		g_AttackEditor->SetFilePath("Resources/Json/Combo/AttackData.json");
		g_AttackEditor->SetReloadCallback([this]() { ReloadAttacks(); });
		g_AttackEditor->SetOpen(false);
	}
}

// ============================================================
// メイン更新処理
// 状態遷移やCC回復の監視を行う
// ============================================================
void PlayerCombo::Update(float deltaTime) {
	previousState_ = currentState_;

	// ------------------------------------------------------------
	// CC回復とコンボ受付タイマーの更新
	// ------------------------------------------------------------
	UpdateCC(deltaTime);
	UpdateComboTimer(deltaTime);
	stateTimer_ += deltaTime;

	// ------------------------------------------------------------
	// 状態ごとの個別更新処理に分岐
	// ------------------------------------------------------------
	switch (currentState_) {

	case ComboState::Attacking:
		UpdateAttacking();
		break;

	case ComboState::CanContinue:
		UpdateCanContinue();
		break;

	case ComboState::Recovery:
		UpdateRecovery();
		break;

	case ComboState::Idle:
		if (GetComboCount() > 0 && comboTimer_ >= config_.comboResetTime) {
			ResetCombo();
		}
		break;

	case ComboState::Finished:
		ChangeState(ComboState::Idle);

		if (onComboEnd_) {
			onComboEnd_(GetComboCount());
		}
		break;
	}
}

// ============================================================
// 攻撃判定の入り口
// ============================================================
bool PlayerCombo::TryAttack(AttackType attackType) {
	// ------------------------------------------------------------
	// 攻撃可能状態かチェック
	// ------------------------------------------------------------
	if (!CanAttack(attackType)) return false;

	AttackData* attack = FindBestAttack(attackType);
	if (!attack) return false;

	// ------------------------------------------------------------
	// コストの確認と攻撃の確定
	// ------------------------------------------------------------
	if (!HasSufficientCC(attack->ccCost)) return false;

	ExecuteAttack(*attack);
	return true;
}

// ============================================================
// 攻撃可能かどうかの判定処理
// ============================================================
bool PlayerCombo::CanAttack([[maybe_unused]] AttackType attackType) const {
	switch (currentState_) {
	case ComboState::Idle:
	case ComboState::CanContinue:
		return true;

	case ComboState::Attacking:
		return currentAttack_ && currentAttack_->canCancel;

	default:
		return false;
	}
}

// ============================================================
// 入力に応じた最適な攻撃データをデータベースから検索する
// 連続同タイプであれば派生攻撃を取り出す
// ============================================================
AttackData* PlayerCombo::FindBestAttack(AttackType type) {
	auto it = attackDatabase_.find(type);
	if (it == attackDatabase_.end() || it->second.empty()) return nullptr;

	const auto& attacks = it->second;

	// ------------------------------------------------------------
	// 初撃と継続中での取得パターンの切り替え
	// ------------------------------------------------------------
	if (GetComboCount() == 0) {
		return const_cast<AttackData*>(&attacks[0]);
	}
	else {
		AttackType lastType = comboChain_.back().type;

		if (lastType == type) {
			int sameTypeCount = 0;
			for (auto combo = comboChain_.rbegin(); combo != comboChain_.rend(); ++combo) {
				if (combo->type == type) sameTypeCount++;
				else break;
			}
			int index = sameTypeCount % static_cast<int>(attacks.size());
			return const_cast<AttackData*>(&attacks[index]);
		}
		else {
			return const_cast<AttackData*>(&attacks[0]);
		}
	}
}

// ============================================================
// 攻撃実行処理
// 必要な情報のセットアップと攻撃用状態への遷移を行う
// ============================================================
void PlayerCombo::ExecuteAttack(const AttackData& attack) {
	// ------------------------------------------------------------
	// 各種パラメータを更新
	// ------------------------------------------------------------
	currentAttack_ = const_cast<AttackData*>(&attack);
	comboChain_.push_back(attack);
	comboDamageMultiplier_ = CalculateDamageMultiplier();

	if (currentState_ == ComboState::Attacking) {
		stateTimer_ = 0.0f; // 同じステートでも強制的にタイマーをリセット
		ccRegenTimer_ = 0.0f;
	}
	else {
		ChangeState(ComboState::Attacking);
	}
	comboTimer_ = 0.0f;

	// ------------------------------------------------------------
	// 攻撃開始/継続コールバックの発火
	// ------------------------------------------------------------
	if (GetComboCount() == 1) {
		if (onAttackStart_) onAttackStart_(attack);
	}
	else {
		if (onAttackContinue_) onAttackContinue_(attack);
	}
}

// ============================================================
// 攻撃状態中の更新処理
// ============================================================
void PlayerCombo::UpdateAttacking() {
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	// 1. 現在のフレーム(int)を計算
	const float frameDuration = (currentAttack_->fps > 0)
		? 1.0f / static_cast<float>(currentAttack_->fps)
		: 1.0f / 60.0f;
	const int currentFrame = static_cast<int>(stateTimer_ / frameDuration);

	// 2. 当たり判定（Hitbox）の有効/無効
	const bool inHitWindow = (currentAttack_->hitEnd > currentAttack_->hitStart)
		&& (currentFrame >= currentAttack_->hitStart)
		&& (currentFrame < currentAttack_->hitEnd);
	if (onSwordColliderChanged_) onSwordColliderChanged_(inHitWindow);

	// ------------------------------------------------------------
	// ★追加：攻撃中のキャンセル（コンボ継続）判定
	// comboWindow のフレーム内なら、ボタンを押した瞬間に次の攻撃へ移行！
	// ------------------------------------------------------------
	if (currentFrame >= currentAttack_->comboWindowStart &&
		currentFrame <= currentAttack_->comboWindowEnd)
	{
		if (owner_) {
			// TryAttack が成功した時点で、ExecuteAttack が呼ばれてステートもタイマーもリセットされる
			if (owner_->IsAttackPressedA()) {
				if (TryAttack(AttackType::A_Arte)) return;
			}
			else if (owner_->IsAttackPressedB()) {
				if (TryAttack(AttackType::B_Arte)) return;
			}
		}
	}

	// ------------------------------------------------------------
	// 3. アニメーションが最後まで終わった場合の処理
	// ------------------------------------------------------------
	if (stateTimer_ >= currentAttack_->duration) {
		// すでに入力受付期間を過ぎているので、硬直(Recovery)へ移行する
		ChangeState(ComboState::Finished);
	}
}

// ============================================================
// コンボ追加入力受付中の更新処理
// ============================================================
void PlayerCombo::UpdateCanContinue() {
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	// 1フレームあたりの時間（秒）を計算
	const float frameDuration = (currentAttack_->fps > 0)
		? 1.0f / static_cast<float>(currentAttack_->fps)
		: 1.0f / 60.0f;

	// 経過時間（stateTimer_）から「現在は何フレーム目か」を割り出す
	const int currentFrame = static_cast<int>(stateTimer_ / frameDuration);

	// フレーム数(int)同士で比較
	if (currentFrame >= currentAttack_->comboWindowStart &&
		currentFrame <= currentAttack_->comboWindowEnd)
	{
		if (owner_) {
			if (owner_->IsAttackPressedA()) {
				if (TryAttack(AttackType::A_Arte)) return;
			}
			else if (owner_->IsAttackPressedB()) {
				if (TryAttack(AttackType::B_Arte)) return;
			}
		}
	}

	// 終了フレームを過ぎたら硬直(Recovery)へ
	if (currentFrame > currentAttack_->comboWindowEnd) {
		ChangeState(ComboState::Recovery);
		return;
	}
}
// ============================================================
// 攻撃硬直中の更新処理
// ============================================================
void PlayerCombo::UpdateRecovery() {
	if (!currentAttack_) { ChangeState(ComboState::Idle); return; }

	if (stateTimer_ >= currentAttack_->recovery)
		ChangeState(ComboState::Finished);
}

// ============================================================
// CC（コスト）の自然回復処理
// ============================================================
void PlayerCombo::UpdateCC(float deltaTime) {
	switch (currentState_) {
	case ComboState::Idle:
		ccRegenTimer_ += deltaTime;
		if (ccRegenTimer_ >= ccConfig_.regenDelay) {
			RecoverCC(static_cast<int>(ccConfig_.regenRate * deltaTime));
		}
		break;

	default:
		ccRegenTimer_ = 0.0f;
		break;
	}
}

// ============================================================
// コンボ時間計測
// ============================================================
void PlayerCombo::UpdateComboTimer(float deltaTime) {
	comboTimer_ += deltaTime;
}

// ============================================================
// 状態の変更処理
// ============================================================
void PlayerCombo::ChangeState(ComboState newState) {
	if (currentState_ == newState) return;

	ExitState(currentState_);
	currentState_ = newState;
	EnterState(newState);
}

// ============================================================
// 各状態への遷移時処理
// ============================================================
void PlayerCombo::EnterState(ComboState newState) {
	stateTimer_ = 0.0f;

	switch (newState) {
	case ComboState::Attacking:
		ccRegenTimer_ = 0.0f;
		break;
	case ComboState::Finished:
		ChangeState(ComboState::Idle);
		if (onComboEnd_) {
			onComboEnd_(GetComboCount());
		}
		break;
	default:
		break;
	}
}

// ============================================================
// 各状態からの退出時処理
// ============================================================
void PlayerCombo::ExitState(ComboState oldState) {
	switch (oldState) {
	case ComboState::Attacking:
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

// ============================================================
// コンボ中のダメージ倍率計算
// 段数による増加とチェインボーナス・減衰の適用を行う
// ============================================================
float PlayerCombo::CalculateDamageMultiplier() const {
	if (GetComboCount() <= 1) return 1.0f;

	float multiplier = 1.0f;

	// ------------------------------------------------------------
	// 基本倍率増加（1コンボにつき +10%）
	// ------------------------------------------------------------
	multiplier += (GetComboCount() - 1) * 0.1f;

	// ------------------------------------------------------------
	// 連携アクションによるボーナス加算
	// ------------------------------------------------------------
	if (GetComboCount() >= 2) {
		const AttackData& prev = comboChain_[comboChain_.size() - 2];
		const AttackData& curr = comboChain_[comboChain_.size() - 1];

		if (IsChainPreferred(prev.type, curr.type)) {
			multiplier *= config_.chainBonus;
		}
	}

	// ------------------------------------------------------------
	// 連続回数によるダメージ減衰ペナルティ
	// ------------------------------------------------------------
	for (int i = 3; i < GetComboCount(); ++i) {
		multiplier *= config_.damageDecay;
	}

	return multiplier;
}

// ============================================================
// 連携推奨パターンのチェック
// ============================================================
bool PlayerCombo::IsChainPreferred(AttackType from, AttackType to) const {
	if (from == AttackType::A_Arte && to == AttackType::B_Arte) return true;
	if (from == AttackType::B_Arte && to == AttackType::A_Arte) return true;
	return false;
}

// ============================================================
// CCの消費処理
// ============================================================
void PlayerCombo::ConsumeCC(int amount) {
	int oldCC = currentCC_;
	currentCC_ = std::max(0, currentCC_ - amount);

	if (onCCChanged_) {
		onCCChanged_(oldCC, currentCC_);
	}
}

// ============================================================
// CCの即時回復処理
// ============================================================
void PlayerCombo::RecoverCC(int amount) {
	int oldCC = currentCC_;
	currentCC_ = std::min(ccConfig_.maxCC, currentCC_ + amount);

	if (onCCChanged_) {
		onCCChanged_(oldCC, currentCC_);
	}
}

// ============================================================
// 各種行動成功時ボーナス
// ============================================================
void PlayerCombo::OnDodgeSuccess() {
	RecoverCC(ccConfig_.dodgeRecovery);
}

void PlayerCombo::OnCounterHit() {
	RecoverCC(ccConfig_.counterRecovery);
}

// ============================================================
// 攻撃ヒット時の前進ステップ処理
// ============================================================
void PlayerCombo::OnHitStep(const Vector3& enemyPosition) {
	if (!currentAttack_) return;
	if (!owner_ || !owner_->GetMovement()) return;

	if (currentAttack_->stepDistance <= 0.0f) return;

	owner_->GetMovement()->RequestAttackStep(enemyPosition, currentAttack_->stepDistance);
}

// ============================================================
// コンボ状態の初期化・中断
// ============================================================
void PlayerCombo::ResetCombo() {
	comboChain_.clear();
	currentAttack_ = nullptr;
	comboDamageMultiplier_ = 1.0f;
	comboTimer_ = 0.0f;

	if (currentState_ != ComboState::Idle) {
		ChangeState(ComboState::Idle);
	}

	if (onComboReset_) {
		onComboReset_();
	}
}

void PlayerCombo::CancelCombo() {
	if (currentAttack_ && currentAttack_->canCancel) {
		ResetCombo();
	}
}

void PlayerCombo::ForceEndCombo() {
	ChangeState(ComboState::Finished);
}

float PlayerCombo::GetComboDamageMultiplier() const {
	return comboDamageMultiplier_;
}

// ============================================================
// 状態の文字列化（デバッグ表示用）
// ============================================================
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

// ============================================================
// 攻撃データベースの読み込み処理
// ============================================================
void PlayerCombo::InitializeAttacks()
{
	const std::string path = "Resources/Json/Combo/AttackData.json";

	if (!AttackDatabase::LoadFromFile(path))
	{
		OutputDebugStringA("[PlayerCombo] AttackData.json load failed!\n");
		return;
	}

	attackDatabase_.clear();
	auto& list = AttackDatabase::Get();

	for (auto& atk : list)
	{
		attackDatabase_[atk.type].push_back(atk);
	}

	currentAttack_ = nullptr;
	comboChain_.clear();
	currentState_ = ComboState::Idle;
	previousState_ = ComboState::Idle;

	stateTimer_ = 0.0f;
	comboTimer_ = 0.0f;
	currentCC_ = ccConfig_.maxCC;
	OutputDebugStringA("[PlayerCombo] AttackData loaded and grouped by AttackType.\n");
}

// ============================================================
// 攻撃データのリロード
// ============================================================
void PlayerCombo::ReloadAttacks() {
	InitializeAttacks();
	OutputDebugStringA("[PlayerCombo] Attack data reloaded!\n");
}

// ============================================================
// デバッグ用UI描画処理
// 現在のCC・コンボチェーン・各種パラメータを表示・操作する
// ============================================================
void PlayerCombo::ShowDebugImGui() {
#ifdef USE_IMGUI
	// ------------------------------------------------------------
	// エディター表示と操作
	// ------------------------------------------------------------
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

	// ------------------------------------------------------------
	// コンボ状態の表示
	// ------------------------------------------------------------
	ImGui::Text("=== コンボ状態 ===");
	ImGui::Text("現在の状態: %s", GetStateString(currentState_));
	ImGui::Text("前の状態: %s", GetStateString(previousState_));

	if (StateChanged()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
		ImGui::Text("状態が変化し���した"); // 絵文字排除
		ImGui::PopStyleColor();
	}

	// ------------------------------------------------------------
	// CCシステムの表示
	// ------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== CCシステム ===");
	ImGui::Text("CC: %d / %d", currentCC_, ccConfig_.maxCC);
	ImGui::ProgressBar(static_cast<float>(currentCC_) / ccConfig_.maxCC, ImVec2(0.0f, 0.0f), "");
	ImGui::Text("CC回復タイマー: %.2f 秒", ccRegenTimer_);

	// ------------------------------------------------------------
	// 連携回数やダメージ倍率の表示
	// ------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== コンボ情報 ===");
	ImGui::Text("コンボ数: %d / %d", GetComboCount(), config_.maxLength);
	ImGui::Text("ダメージ倍率: x%.2f", GetComboDamageMultiplier());
	ImGui::Text("コンボタイマー: %.2f 秒", comboTimer_);
	ImGui::Text("状態タイマー: %.2f 秒", stateTimer_);

	// ------------------------------------------------------------
	// 攻撃データの読み込み状況表示
	// ------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== 攻撃データベース ===");
	ImGui::Text("A攻撃数: %d", static_cast<int>(attackDatabase_[AttackType::A_Arte].size()));
	ImGui::Text("B攻撃数: %d", static_cast<int>(attackDatabase_[AttackType::B_Arte].size()));

	// ------------------------------------------------------------
	// 実行中の攻撃情報表示
	// ------------------------------------------------------------
	if (currentAttack_) {
		ImGui::Separator();
		ImGui::Text("=== 現在の攻撃 ===");
		ImGui::Text("攻撃名: %s", currentAttack_->name.c_str());
		ImGui::Text("アニメーション: %s", currentAttack_->animationName.c_str());
		ImGui::Text("ダメージ: %.1f", currentAttack_->baseDamage);
		ImGui::Text("CC消費: %d", currentAttack_->ccCost);
		ImGui::Text("キャンセル可能: %s", currentAttack_->canCancel ? "はい" : "いいえ");
		ImGui::Text("自由チェーン: %s", currentAttack_->canChainToAny ? "はい" : "いいえ");

		const char* typeStr = "";
		switch (currentAttack_->type) {
		case AttackType::A_Arte: typeStr = "A"; break;
		case AttackType::B_Arte: typeStr = "B"; break;
		}
		ImGui::Text("タイプ: %s", typeStr);
	}

	// ------------------------------------------------------------
	// チェーン履歴の表示
	// ------------------------------------------------------------
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

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("ダメージ: %.1f", attack.baseDamage);
				ImGui::Text("CC消費: %d", attack.ccCost);
				ImGui::Text("持続時間: %.2f秒", attack.duration);
				ImGui::EndTooltip();
			}
		}
	}

	// ------------------------------------------------------------
	// 強制リセット・攻撃テスト用ボタン
	// ------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("=== 操作テスト ===");

	if (ImGui::Button("コンボリセット")) ResetCombo();
	ImGui::SameLine();
	if (ImGui::Button("強制終了")) ForceEndCombo();
	ImGui::SameLine();
	if (ImGui::Button("CC全回復")) currentCC_ = ccConfig_.maxCC;

	ImGui::Separator();
	if (ImGui::Button("A攻撃")) TryAttack(AttackType::A_Arte);
	ImGui::SameLine();
	if (ImGui::Button("B攻撃")) TryAttack(AttackType::B_Arte);
#endif
}

// ============================================================
// Json管理オブジェクトへの登録処理
// ============================================================
void PlayerCombo::InitJson(YoRigine::JsonManager* jsonManager) {
	// ------------------------------------------------------------
	// CC回復システムの基本設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("CCの設定");
	jsonManager->Register("最大CCの値", &ccConfig_.maxCC);
	jsonManager->Register("CC回復速度（秒）", &ccConfig_.regenRate);
	jsonManager->Register("攻撃後のCC回復開始遅延", &ccConfig_.regenDelay);
	jsonManager->Register("回避成功時のCC回復量", &ccConfig_.dodgeRecovery);
	jsonManager->Register("カウンター成功時のCC回復量", &ccConfig_.counterRecovery);

	// ------------------------------------------------------------
	// コンボ制限やダメージ倍率に関する設定
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("コンボの設定");
	jsonManager->Register("最大コンボ長", &config_.maxLength);
	jsonManager->Register("ダメージ減衰率", &config_.damageDecay);
	jsonManager->Register("チェーンボーナス倍率", &config_.chainBonus);
	jsonManager->Register("自由チェーン有効", &config_.enableFreeChain);
	jsonManager->Register("コンボリセット時間（秒）", &config_.comboResetTime);
}
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
	// インゲームエディターの生成とコールバック登録
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
// UI用のタイマーや、CCの自然回復などを管理する
// ============================================================
void PlayerCombo::Update(float deltaTime) {
	previousState_ = currentState_;
	stateTimer_ += deltaTime;

	UpdateCC(deltaTime);
	UpdateComboTimer(deltaTime);
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

	//ConsumeCC(attack->ccCost); // 先にCCを消費する
	ExecuteAttack(*attack);
	return true;
}

// ============================================================
// 攻撃可能かどうかの判定処理
// ============================================================
bool PlayerCombo::CanAttack([[maybe_unused]] AttackType attackType) const {
	// 待機中なら攻撃可能、攻撃中ならキャンセル可能フラグを見る
	if (currentAttack_ == nullptr) {
		return true;
	}
	return currentAttack_->canCancel;
}

// ============================================================
// 入力に応じた最適な攻撃データをデータベースから検索する
// 連続同タイプであれば派生攻撃を取り出す
// ============================================================
AttackData* PlayerCombo::FindBestAttack(AttackType type) {
	auto it = attackDatabase_.find(type);
	if (it == attackDatabase_.end() || it->second.empty()) return nullptr;

	const auto& attacks = it->second;

	// 現在の段数 = いまから行う攻撃が何段目か
	int comboStep = GetComboCount();

	// 段数が攻撃データ数以上なら最後のデータ（フィニッシュ）を返す
	if (comboStep >= static_cast<int>(attacks.size())) {
		return const_cast<AttackData*>(&attacks.back());
	}
	else {
		return const_cast<AttackData*>(&attacks[comboStep]);
	}
}

// ============================================================
// 攻撃実行処理
// 必要な情報のセットアップとデータ更新を行う
// ============================================================
void PlayerCombo::ExecuteAttack(const AttackData& attack) {
	// ------------------------------------------------------------
	// 各種パラメータを更新
	// ------------------------------------------------------------
	currentAttack_ = const_cast<AttackData*>(&attack);
	comboChain_.push_back(attack);
	comboDamageMultiplier_ = CalculateDamageMultiplier();

	previousState_ = currentState_;
	currentState_ = ComboState::Attacking;

	stateTimer_ = 0.0f;
	comboTimer_ = 0.0f;

	// ------------------------------------------------------------
	// 攻撃開始/継続コールバックの発火
	// （これにより AttackingCombatState 側の処理が更新される）
	// ------------------------------------------------------------
	if (GetComboCount() == 1) {
		if (onAttackStart_) onAttackStart_(attack);
	}
	else {
		if (onAttackContinue_) onAttackContinue_(attack);
	}
}

// ============================================================
// 攻撃アニメーション終了通知（AttackingCombatStateから呼ばれる）
// ============================================================
void PlayerCombo::OnAttackFinished() {
	currentAttack_ = nullptr;
	previousState_ = currentState_;
	currentState_ = ComboState::Idle;
	stateTimer_ = 0.0f;
	// ※ comboTimer_ はここでリセットせず、コンボが途切れるまで計測を続ける
}

// ============================================================
// CC（コスト）の自然回復処理
// ============================================================
void PlayerCombo::UpdateCC(float deltaTime) {
	// 攻撃中でなければ（待機中なら）回復タイマーを進める
	if (currentAttack_ == nullptr) {
		ccRegenTimer_ += deltaTime;
		if (ccRegenTimer_ >= ccConfig_.regenDelay) {
			RecoverCC(static_cast<int>(ccConfig_.regenRate * deltaTime));
			// 必要に応じて ccRegenTimer_ = 0.0f; とする設計もありますが、
			// 元のロジックに従いタイマーは累積し続ける形にしています。
		}
	}
	else {
		ccRegenTimer_ = 0.0f;
	}
}

// ============================================================
// コンボ時間計測と自動リセット
// ============================================================
void PlayerCombo::UpdateComboTimer(float deltaTime) {
	// 待機中かつコンボ履歴が残っている場合のみ計測
	if (currentAttack_ == nullptr && !comboChain_.empty()) {
		comboTimer_ += deltaTime;
		if (comboTimer_ >= config_.comboResetTime) {
			ResetCombo();
		}
	}
	else if (currentAttack_ != nullptr) {
		comboTimer_ = 0.0f;
	}
}

// ============================================================
// コンボ中のダメージ倍率計算
// ============================================================
float PlayerCombo::CalculateDamageMultiplier() const {
	if (GetComboCount() <= 1) return 1.0f;

	float multiplier = 1.0f;

	// 基本倍率増加（1コンボにつき +10%）
	multiplier += (GetComboCount() - 1) * 0.1f;

	// 連携アクションによるボーナス加算
	if (GetComboCount() >= 2) {
		const AttackData& prev = comboChain_[comboChain_.size() - 2];
		const AttackData& curr = comboChain_[comboChain_.size() - 1];

		if (IsChainPreferred(prev.type, curr.type)) {
			multiplier *= config_.chainBonus;
		}
	}

	// 連続回数によるダメージ減衰ペナルティ
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
	if (!comboChain_.empty() && onComboEnd_) {
		onComboEnd_(GetComboCount());
	}

	comboChain_.clear();
	currentAttack_ = nullptr;
	comboDamageMultiplier_ = 1.0f;
	comboTimer_ = 0.0f;

	previousState_ = currentState_;
	currentState_ = ComboState::Idle;
	stateTimer_ = 0.0f;

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
	ResetCombo();
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
		ImGui::Text("状態が変化しました");
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
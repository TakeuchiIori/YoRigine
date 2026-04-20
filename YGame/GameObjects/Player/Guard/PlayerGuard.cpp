#include "PlayerGuard.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// Engine
#include <Loaders/Json/JsonManager.h>

// ============================================================
// コンストラクタ
// ============================================================
PlayerGuard::PlayerGuard(Player* owner)
{
	owner_ = owner;
}

// ============================================================
// JSON設定ファイルの初期化
// ============================================================
void PlayerGuard::InitJson(YoRigine::JsonManager* jsonManager) {
	// ------------------------------------------------------------
	// 外部から調整可能な各種ガードフレームの登録
	// ------------------------------------------------------------
	jsonManager->SetTreePrefix("ガード");
	jsonManager->Register("通常ガードの時間", &gc_.active);
	jsonManager->Register("ボタン押してガードが有効になる時間", &gc_.startup);
	jsonManager->Register("パリィ開始時間", &gc_.parryStart);
	jsonManager->Register("パリィ終了時間", &gc_.parryEnd);
}

// ============================================================
// ガードの開始処理
// ============================================================
bool PlayerGuard::StartGuard()
{
	if (state_ != State::Idle) return false;

	ChangeState(State::StartUp);
	return true;
}

// ============================================================
// 更新処理
// ガードステートに応じたフレーム進行を行う
// ============================================================
void PlayerGuard::Update([[maybe_unused]] float deltaTime)
{
	switch (state_) {
	case State::StartUp:
		if (++frame_ >= gc_.startup)
			ChangeState(State::Active);
		break;

	case State::Active:
		if (++frame_ >= gc_.active)
			ChangeState(State::Recovery);
		break;

	case State::Recovery:
		if (++frame_ >= gc_.recovery)
			ChangeState(State::Idle);
		break;

	default:
		break;
	}
}

// ============================================================
// 状態のリセット
// ============================================================
void PlayerGuard::Reset()
{
	ChangeState(State::Idle);
}

// ============================================================
// 攻撃被弾時のガード判定処理
// ============================================================
PlayerGuard::GuardResult PlayerGuard::OnHit([[maybe_unused]] BaseCollider* other)
{
	// ------------------------------------------------------------
	// ガード判定が発生していない場合はガード失敗
	// ------------------------------------------------------------
	if (state_ != State::Active) {
		if (onGuardFail_) onGuardFail_();
		ChangeState(State::Idle);
		return GuardResult::GuardFail;
	}

	// ------------------------------------------------------------
	// パリィ判定期間か、通常ガードかを判別して結果を返す
	// ------------------------------------------------------------
	if (IsParryWindow()) {
		if (onParrySuccess_) onParrySuccess_();
		ChangeState(State::Recovery);
		return GuardResult::ParrySuccess;
	}
	else {
		if (onGuardSuccess_) onGuardSuccess_();
		ChangeState(State::Recovery);
		return GuardResult::GuardSuccess;
	}
}

// ============================================================
// デバッグ用UIの表示
// ============================================================
void PlayerGuard::ShowDebugImGui()
{
#ifdef USE_IMGUI
	if (ImGui::BeginTabBar("ガード情報"))
	{
		// ------------------------------------------------------------
		// ステートとフレームの表示
		// ------------------------------------------------------------
		const char* stateNames[] = { "Idle", "スタートアップ", "アクティブ", "リカバリー" };
		ImGui::Text("現在のステート: %s", stateNames[static_cast<int>(state_)]);
		ImGui::Text("現在のフレーム数: %.1f", frame_);

		ImGui::Separator();

		// ------------------------------------------------------------
		// ガード関連のフレーム設定をリアルタイムで変更する
		// ------------------------------------------------------------
		ImGui::Text("■ ガード設定");
		ImGui::InputFloat("スタートアップ時間 (フレーム)", &gc_.startup, 1.0f, 5.0f);
		ImGui::InputFloat("アクティブ時間 (フレーム)", &gc_.active, 1.0f, 5.0f);
		ImGui::InputFloat("パリィ開始フレーム", &gc_.parryStart, 1.0f, 1.0f);
		ImGui::InputFloat("パリィ終了フレーム", &gc_.parryEnd, 1.0f, 1.0f);
		ImGui::InputFloat("リカバリー時間 (フレーム)", &gc_.recovery, 1.0f, 5.0f);

		ImGui::Separator();

		ImGui::Text("パリィウィンドウ中: %s", IsParryWindow() ? "はい" : "いいえ");
	}
	ImGui::EndTabBar();
#endif
}

// ============================================================
// 状態の変更処理
// ============================================================
void PlayerGuard::ChangeState(State s)
{
	State prev = state_;
	state_ = s;
	frame_ = 0;

	if (onStateChanged_) onStateChanged_(prev, s);
}
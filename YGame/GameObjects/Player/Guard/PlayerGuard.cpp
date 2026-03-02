#include "PlayerGuard.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG

// Engine
#include <Loaders/Json/JsonManager.h>

/// <summary>
/// コンストラクタ
/// </summary>
/// <param name="owner">プレイヤーへのポインタ</param>
PlayerGuard::PlayerGuard(Player* owner)
{
	owner_ = owner;
}

/// <summary>
/// YoRigine::JsonManager によるパラメータ登録
/// ガード関連の調整値を外部から編集可能にする
/// </summary>
/// <param name="jsonManager">Json管理オブジェクト</param>
void PlayerGuard::InitJson(YoRigine::JsonManager* jsonManager) {

	//---------------------------------------------------------------------------------------------
	// ガード関連パラメータ登録
	//---------------------------------------------------------------------------------------------
	jsonManager->SetTreePrefix("ガード");
	jsonManager->Register("通常ガードの時間", &gc_.active);
	jsonManager->Register("ボタン押してガードが有効になる時間", &gc_.startup);
	jsonManager->Register("パリィ開始時間", &gc_.parryStart);
	jsonManager->Register("パリィ終了時間", &gc_.parryEnd);
}

/// <summary>
/// ガード開始処理
/// </summary>
/// <returns>ガードが開始されたか</returns>
bool PlayerGuard::StartGuard()
{
	//---------------------------------------------------------------------------------------------
	// Idle 状態のみガード開始可能
	//---------------------------------------------------------------------------------------------
	if (state_ != State::Idle) return false;

	// スタートアップ状態へ遷移
	ChangeState(State::StartUp);
	return true;
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
/// <param name="deltaTime">経過時間</param>
void PlayerGuard::Update([[maybe_unused]] float deltaTime)
{
	//---------------------------------------------------------------------------------------------
	// 現在の状態ごとの処理
	//---------------------------------------------------------------------------------------------
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

/// <summary>
/// 状態をリセット（強制的に Idle に戻す）
/// </summary>
void PlayerGuard::Reset()
{
	ChangeState(State::Idle);
}

/// <summary>
/// 攻撃を受けた際の処理
/// </summary>
/// <param name="other">攻撃元コライダー</param>
/// <returns>ガード結果</returns>
PlayerGuard::GuardResult PlayerGuard::OnHit([[maybe_unused]] BaseCollider* other)
{
	//---------------------------------------------------------------------------------------------
	// ガード判定が出ていない状態で被弾した場合 → 失敗
	//---------------------------------------------------------------------------------------------
	if (state_ != State::Active) {
		if (onGuardFail_) onGuardFail_();
		ChangeState(State::Idle);
		return GuardResult::GuardFail;
	}

	//---------------------------------------------------------------------------------------------
	// パリィウィンドウ中の被弾 → 完璧ガード
	//---------------------------------------------------------------------------------------------
	if (IsParryWindow()) {
		if (onParrySuccess_) onParrySuccess_();
		ChangeState(State::Recovery);
		return GuardResult::ParrySuccess;
	} else {
		// 通常ガード成功
		if (onGuardSuccess_) onGuardSuccess_();
		ChangeState(State::Recovery);
		return GuardResult::GuardSuccess;
	}
}

/// <summary>
/// ImGui デバッグ表示
/// ガードステートや設定値をリアルタイム確認・編集可能にする
/// </summary>
void PlayerGuard::ShowDebugImGui()
{
#ifdef USE_IMGUI
	if (ImGui::BeginTabBar("ガード情報"))
	{

		//---------------------------------------------------------------------------------------------
		// 現在ステート表示
		//---------------------------------------------------------------------------------------------
		const char* stateNames[] = { "Idle", "スタートアップ", "アクティブ", "リカバリー" };
		ImGui::Text("現在のステート: %s", stateNames[static_cast<int>(state_)]);

		//---------------------------------------------------------------------------------------------
		// 現在の経過フレーム表示
		//---------------------------------------------------------------------------------------------
		ImGui::Text("現在のフレーム数: %.1f", frame_);

		ImGui::Separator();

		//---------------------------------------------------------------------------------------------
		// 設定値のリアルタイム編集
		//---------------------------------------------------------------------------------------------
		ImGui::Text("■ ガード設定");
		ImGui::InputFloat("スタートアップ時間 (フレーム)", &gc_.startup, 1.0f, 5.0f);
		ImGui::InputFloat("アクティブ時間 (フレーム)", &gc_.active, 1.0f, 5.0f);
		ImGui::InputFloat("パリィ開始フレーム", &gc_.parryStart, 1.0f, 1.0f);
		ImGui::InputFloat("パリィ終了フレーム", &gc_.parryEnd, 1.0f, 1.0f);
		ImGui::InputFloat("リカバリー時間 (フレーム)", &gc_.recovery, 1.0f, 5.0f);

		ImGui::Separator();

		//---------------------------------------------------------------------------------------------
		// パリィウィンドウ情報
		//---------------------------------------------------------------------------------------------
		ImGui::Text("パリィウィンドウ中: %s", IsParryWindow() ? "はい" : "いいえ");
	}
	ImGui::EndTabBar();
#endif // _DEBUG
}

/// <summary>
/// 状態変更処理
/// </summary>
/// <param name="s">新しい状態</param>
void PlayerGuard::ChangeState(State s)
{
	State prev = state_;
	state_ = s;
	frame_ = 0;

	//---------------------------------------------------------------------------------------------
	// コールバック呼び出し（デバッグ・イベント通知）
	//---------------------------------------------------------------------------------------------
	if (onStateChanged_) onStateChanged_(prev, s);
}

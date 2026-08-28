#include "PlayerGuard.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include "../Player.h"

#include <algorithm>
#include <cmath>
#include <numbers>

// ============================================================
// コンストラクタ
// ============================================================
PlayerGuard::PlayerGuard(Player *owner) : owner_(owner) {}

// ============================================================
// 設定ファイルの読み書き
// ============================================================
void PlayerGuard::LoadConfig(const std::string &path) {
	GuardConfigIO::Load(path, config_);
	config_.timeline.Sanitize();
}

bool PlayerGuard::SaveConfig(const std::string &path) const {
	return GuardConfigIO::Save(path, config_);
}

// ============================================================
// ガードの開始処理
// ============================================================
bool PlayerGuard::StartGuard() {
	if (state_ != State::Idle) return false;

	ChangeState(State::StartUp);
	return true;
}

// ============================================================
// 更新処理
// フェーズごとの経過フレームを進め、順に遷移させる
// ============================================================
void PlayerGuard::Update([[maybe_unused]] float deltaTime) {
	const GuardTimeline &tl = config_.timeline;

	switch (state_) {
	case State::StartUp:
		if (++frame_ >= tl.startupFrames) ChangeState(State::Active);
		break;

	case State::Active:
		if (++frame_ >= tl.activeFrames) ChangeState(State::Recovery);
		break;

	case State::Recovery:
		if (++frame_ >= tl.recoveryFrames) ChangeState(State::Idle);
		break;

	default:
		break;
	}
}

// ============================================================
// 状態のリセット
// ============================================================
void PlayerGuard::Reset() { ChangeState(State::Idle); }

// ============================================================
// タイムライン先頭からの通算フレーム
// エディタの再生ヘッド表示に使う
// ============================================================
int PlayerGuard::GetTimelineFrame() const {
	const GuardTimeline &tl = config_.timeline;
	switch (state_) {
	case State::StartUp:  return frame_;
	case State::Active:   return tl.startupFrames + frame_;
	case State::Recovery: return tl.startupFrames + tl.activeFrames + frame_;
	default:              return -1; // 待機中はヘッドを出さない
	}
}

// ============================================================
// 攻撃が正面から来たか
// 背後からの攻撃まで防げると、ガードを固めるだけで無敵になってしまう
// ============================================================
bool PlayerGuard::IsAttackFromFront(const Vector3 &attackerPosition) const {
	if (!owner_) return true;

	Vector3 toAttacker = attackerPosition - owner_->GetWorldPosition();
	toAttacker.y = 0.0f;
	if (Length(toAttacker) < 0.001f) return true; // 完全に重なっているときは正面扱い
	toAttacker = Normalize(toAttacker);

	const float yaw = owner_->GetRotate().y;
	const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};

	const float dot = std::clamp(Dot(forward, toAttacker), -1.0f, 1.0f);
	const float angleDeg = std::acos(dot) * 180.0f / std::numbers::pi_v<float>;
	return angleDeg <= config_.frontHalfAngleDeg;
}

// ============================================================
// 攻撃を受けた瞬間の防御判定
//
// ここがダメージ処理から呼ばれる唯一の判断点。
// 判定後は Recovery へ移して、1回のガードで受けられるのを1発に限定する。
// ============================================================
PlayerGuard::GuardResult PlayerGuard::ResolveHit(const Vector3 &attackerPosition) {
	// 発生前・硬直中・待機中はそもそも防げない
	if (state_ != State::Active) {
		if (onGuardFail_) onGuardFail_();
		return GuardResult::GuardFail;
	}

	// 背後からの攻撃は防げない。ガードは解除して硬直に入る。
	if (!IsAttackFromFront(attackerPosition)) {
		if (onGuardFail_) onGuardFail_();
		ChangeState(State::Recovery);
		return GuardResult::GuardFail;
	}

	// パリィ受付中なら弾き返し、そうでなければ通常ガード
	const bool isParry = IsParryWindow();
	ChangeState(State::Recovery);

	if (isParry) {
		if (onParrySuccess_) onParrySuccess_();
		return GuardResult::ParrySuccess;
	}

	if (onGuardSuccess_) onGuardSuccess_();
	return GuardResult::GuardSuccess;
}

// ============================================================
// 判定結果に対応する結果パラメータ
// ============================================================
const GuardOutcome *PlayerGuard::GetOutcome(GuardResult result) const {
	switch (result) {
	case GuardResult::GuardSuccess: return &config_.guard;
	case GuardResult::ParrySuccess: return &config_.parry;
	default:                        return nullptr;
	}
}

// ============================================================
// 状態の変更処理
// ============================================================
void PlayerGuard::ChangeState(State s) {
	const State prev = state_;
	state_ = s;
	frame_ = 0;

	if (onStateChanged_) onStateChanged_(prev, s);
}

// ============================================================
// デバッグ用UIの表示
// 数値の編集はドープシート付きの GuardEditor 側で行うので、
// ここでは現在の進行状況だけを見せる。
// ============================================================
void PlayerGuard::ShowDebugImGui() {
#ifdef USE_IMGUI
	const char *stateNames[] = {"待機", "発生前", "防御中", "硬直"};
	ImGui::Text("状態: %s  (%dフレーム目)", stateNames[static_cast<int>(state_)], frame_);

	if (IsParryWindow()) {
		ImGui::TextColored({1.0f, 0.9f, 0.2f, 1.0f}, "パリィ受付中");
	} else if (state_ == State::Active) {
		ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "通常ガード成立中");
	} else {
		ImGui::TextDisabled("防御は成立しない");
	}
#endif
}

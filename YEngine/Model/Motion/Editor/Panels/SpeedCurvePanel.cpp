#include "SpeedCurvePanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <ImCurveEdit.h>
#endif

#include "MotionEditorContext.h"
#include "../../Core/Motion.h"

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
void SpeedCurvePanel::Initialize(MotionEditorContext* context)
{
	context_ = context;
}

// -----------------------------------------------------------------------
// DrawImGui
// -----------------------------------------------------------------------
void SpeedCurvePanel::DrawImGui()
{
#ifdef USE_IMGUI
	if (!context_) return;

	Motion* motion = context_->currentMotion;

	// ── モーションが切り替わったら制御点を再取得 ──────────────────
	if (motion != lastMotion_) {
		lastMotion_ = motion;
		PullFromMotion();
		isDirty_ = false;
	}

	ImGui::PushID("SpeedCurvePanel");

	// ── ヘッダ ────────────────────────────────────────────────────
	ImGui::SeparatorText("\uf0e7 タイムスケールカーブ");

	if (!motion) {
		ImGui::TextDisabled("モーションが選択されていません");
		ImGui::PopID();
		return;
	}

	// ── Y軸上限スライダ ───────────────────────────────────────────
	if (ImGui::SliderFloat("速度上限##maxspd", &maxSpeedEdit_, 1.0f, 8.0f, "%.1f x")) {
		delegate_.SetMaxSpeed(maxSpeedEdit_);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(1.0 = 等速)");

	// ── カーブエディタ ────────────────────────────────────────────
	ImVec2 curveSize = ImVec2(ImGui::GetContentRegionAvail().x, 160.0f);

	// ImCurveEdit::Edit() は変更があると true を返す
	if (ImCurveEdit::Edit(delegate_, curveSize, ImGui::GetID("##speedcurve"), nullptr, &editInfo_)) {
		isDirty_ = true;
	}

	// X軸ラベル（簡易）
	{
		float w = curveSize.x;
		ImVec2 base = ImGui::GetItemRectMin();
		ImVec2 bot  = ImGui::GetItemRectMax();
		auto* dl = ImGui::GetWindowDrawList();
		auto col = IM_COL32(160, 160, 160, 180);
		float y = bot.y + 2.0f;
		dl->AddText(ImVec2(base.x,          y), col, "0");
		dl->AddText(ImVec2(base.x + w*0.5f - 4.0f, y), col, "0.5");
		dl->AddText(ImVec2(base.x + w - 8.0f, y), col, "1");
	}
	ImGui::Spacing(); ImGui::Spacing();

	// ── 制御点の追加ヒント ────────────────────────────────────────
	ImGui::TextDisabled("右クリック: 制御点追加 / Ctrl+クリック: 削除");

	ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

	// ── ボタン行 ──────────────────────────────────────────────────

	// [ランタイム適用] カーブをMotionに書き戻すがキーフレームは変更しない
	bool canApply = isDirty_;
	if (!canApply) ImGui::BeginDisabled();
	if (ImGui::Button("\uf0e7 ランタイム適用", ImVec2(140, 0))) {
		PushToMotion();
		isDirty_ = false;
		context_->statusMsg = "スピードカーブをランタイム適用しました";
	}
	if (!canApply) ImGui::EndDisabled();

	ImGui::SameLine(0, 8);

	// [焼き込み] キーフレームの時間を再分配してカーブをクリア
	if (ImGui::Button("\uf0c7 焼き込み (Bake)", ImVec2(150, 0))) {
		PushToMotion();
		BakeSpeedCurve();
		PullFromMotion(); // 焼き込み後はカーブがクリアされるので再Pull
		isDirty_ = false;
		context_->statusMsg = "スピードカーブを焼き込みました";
		context_->requireTimelineRebuild = true;
		context_->lastAppliedScrubTime   = -1.0f;
	}

	ImGui::SameLine(0, 8);

	// [リセット] カーブをクリア（等速に戻す）
	if (ImGui::Button("\uf0e2 リセット", ImVec2(90, 0))) {
		if (motion) {
			motion->ClearSpeedCurve();
			PullFromMotion();
			isDirty_ = false;
			context_->statusMsg = "スピードカーブをリセットしました";
		}
	}

	// ── 現在時刻での速度倍率プレビュー ───────────────────────────
	if (motion && motion->GetDuration() > 0.0f) {
		float nt = context_->scrubTime / motion->GetDuration();
		nt = std::clamp(nt, 0.0f, 1.0f);
		float spd = delegate_.points.empty()
			? 1.0f
			: motion->EvaluateSpeedCurve(nt);

		ImGui::Spacing();
		ImGui::Text("現在位置の速度倍率: %.2f x  (正規化時間: %.3f)", spd, nt);
	}

	ImGui::PopID();
#endif
}

// -----------------------------------------------------------------------
// PullFromMotion  Motion::SpeedCurve → delegate_.points
// -----------------------------------------------------------------------
void SpeedCurvePanel::PullFromMotion()
{
#ifdef USE_IMGUI
	delegate_.points.clear();

	Motion* motion = context_ ? context_->currentMotion : nullptr;
	if (!motion) return;

	const auto& kfs = motion->GetSpeedCurve().curve.keyframes;
	if (kfs.empty()) {
		// デフォルト: 等速の2点
		delegate_.points = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
		return;
	}

	for (const auto& kf : kfs) {
		delegate_.points.push_back({ kf.time, kf.value });
	}
#endif
}

// -----------------------------------------------------------------------
// PushToMotion  delegate_.points → Motion::SpeedCurve
// -----------------------------------------------------------------------
void SpeedCurvePanel::PushToMotion()
{
#ifdef USE_IMGUI
	Motion* motion = context_ ? context_->currentMotion : nullptr;
	if (!motion) return;

	auto& kfs = motion->GetSpeedCurve().curve.keyframes;
	kfs.clear();

	for (const auto& p : delegate_.points) {
		Motion::Keyframe<float> kf;
		kf.time  = p.x;
		kf.value = p.y;
		kfs.push_back(kf);
	}

	// 時間順にソート
	std::sort(kfs.begin(), kfs.end(),
		[](const Motion::Keyframe<float>& a, const Motion::Keyframe<float>& b) {
			return a.time < b.time;
		});
#endif
}

// -----------------------------------------------------------------------
// BakeSpeedCurve
// SpeedCurveをキーフレームの時間軸に焼き込む
//
// アルゴリズム:
//   1. SpeedCurve を数値積分して normalizedTime -> bakedTime のLUTを作成
//      (bakedTime = integral_0^t speed(u) du を正規化したもの)
//   2. 各NodeAnimationのキーフレーム時間を LUT でリマップ
//   3. SpeedCurveをクリア
// -----------------------------------------------------------------------
void SpeedCurvePanel::BakeSpeedCurve()
{
	Motion* motion = context_ ? context_->currentMotion : nullptr;
	if (!motion || !motion->HasSpeedCurve()) return;

	const float duration = motion->GetDuration();
	if (duration <= 0.0f) return;

	// ── Step1: 数値積分 (台形則) で accumulated speed LUT を作成 ──
	constexpr int kSamples = 512;
	std::vector<float> lut(kSamples + 1); // lut[i] = 積算速度 at normalized t = i/kSamples

	lut[0] = 0.0f;
	for (int i = 1; i <= kSamples; ++i) {
		float t0 = static_cast<float>(i - 1) / kSamples;
		float t1 = static_cast<float>(i)     / kSamples;
		float s0 = motion->EvaluateSpeedCurve(t0);
		float s1 = motion->EvaluateSpeedCurve(t1);
		lut[i] = lut[i - 1] + (s0 + s1) * 0.5f / kSamples; // 台形則
	}
	float totalAccum = lut[kSamples];
	if (totalAccum < 1e-6f) return; // ゼロ除算ガード

	// ── Step2: 各キーフレームの時間をリマップ ──────────────────────
	// キーフレームの normalized_t -> new_normalized_t
	// new_normalized_t = lut の逆引き (lut[i]/totalAccum = new_t)
	auto remapTime = [&](float origNormT) -> float {
		// origNormT に対応する lut 値
		float targetAccum = origNormT * totalAccum;
		// LUTを逆引き (二分探索)
		int lo = 0, hi = kSamples;
		while (lo < hi) {
			int mid = (lo + hi) / 2;
			if (lut[mid] < targetAccum) lo = mid + 1;
			else hi = mid;
		}
		if (lo == 0) return 0.0f;
		if (lo > kSamples) return 1.0f;
		float t0 = static_cast<float>(lo - 1) / kSamples;
		float t1 = static_cast<float>(lo)     / kSamples;
		float v0 = lut[lo - 1], v1 = lut[lo];
		float alpha = (v1 > v0) ? (targetAccum - v0) / (v1 - v0) : 0.0f;
		return t0 + alpha * (t1 - t0);
	};

	for (auto& [boneName, nodeAnim] : motion->animation_.nodeAnimations_) {
		auto remap = [&](auto& track) {
			for (auto& kf : track.keyframes) {
				float nt = kf.time / duration;
				kf.time  = remapTime(nt) * duration;
			}
		};
		remap(nodeAnim.translate);
		remap(nodeAnim.rotate);
		remap(nodeAnim.scale);
	}

	// ── Step3: SpeedCurveをクリア ─────────────────────────────────
	motion->ClearSpeedCurve();
}

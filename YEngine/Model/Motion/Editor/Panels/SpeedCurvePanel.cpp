#include "SpeedCurvePanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <ImCurveEdit.h>
#endif

#include "../MotionEditorContext.h"
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

	// モーションが切り替わったら制御点を再取得
	if (motion != lastMotion_) {
		lastMotion_ = motion;
		hasBakedSnapshot_ = false;
		showBakedPreview_ = false;
		PullFromMotion();
		isDirty_ = false;
		selection_.clear();
	}

	ImGui::PushID("SpeedCurvePanel");

	// ============================================================
	// CollapsingHeader でコンパクト化
	// ============================================================
	// タイトルに状態をインライン表示
	char headerLabel[128];
	if (hasBakedSnapshot_) {
		snprintf(headerLabel, sizeof(headerLabel), "\uf0e7 タイムスケールカーブ  [焼込済]###SpeedCurveHeader");
	}
	else if (isDirty_) {
		snprintf(headerLabel, sizeof(headerLabel), "\uf0e7 タイムスケールカーブ  *###SpeedCurveHeader");
	}
	else {
		snprintf(headerLabel, sizeof(headerLabel), "\uf0e7 タイムスケールカーブ###SpeedCurveHeader");
	}

	bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen);

	if (!open) {
		// 折りたたみ中でも速度プレビューだけをツールチップで見せる
		if (motion && motion->GetDuration() > 0.0f && ImGui::IsItemHovered()) {
			float nt = std::clamp(context_->scrubTime / motion->GetDuration(), 0.0f, 1.0f);
			float spd = motion->EvaluateSpeedCurve(nt);
			ImGui::SetTooltip("現在速度倍率: %.2f x", spd);
		}
		ImGui::PopID();
		return;
	}

	// ============================================================
	// モーション未選択
	// ============================================================
	if (!motion) {
		ImGui::TextDisabled("モーションが選択されていません");
		ImGui::PopID();
		return;
	}

	// ============================================================
	// 速度上限スライダ（コンパクト：幅を制限）
	// ============================================================
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::SliderFloat("速度上限##maxspd", &maxSpeedEdit_, 1.0f, 8.0f, "%.1fx")) {
		delegate_.SetMaxSpeed(maxSpeedEdit_);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(1.0=等速)");

	// 焼き込み済みなら右端にオーバーレイ表示トグルを出す
	if (hasBakedSnapshot_) {
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 130.0f);
		ImGui::Checkbox("\uf0c7 焼込前を表示##bkprev", &showBakedPreview_);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("焼き込む前のカーブを\nオレンジ色で重ねて表示します");
		}
	}

	// ============================================================
	// カーブエディタ本体
	// ============================================================
	ImVec2 curveSize = ImVec2(ImGui::GetContentRegionAvail().x, 150.0f);

	if (ImCurveEdit::Edit(delegate_, curveSize, ImGui::GetID("##speedcurve"), nullptr, &selection_)) {
		isDirty_ = true;
	}

	// 焼き込み前スナップショットのオーバーレイ
	if (hasBakedSnapshot_ && showBakedPreview_) {
		DrawBakedPreviewOverlay(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	}

	// X軸ラベル
	{
		ImVec2 base = ImGui::GetItemRectMin();
		ImVec2 bot = ImGui::GetItemRectMax();
		float  w = bot.x - base.x;
		auto* dl = ImGui::GetWindowDrawList();
		ImU32  col = IM_COL32(150, 150, 150, 160);
		float  y = bot.y + 2.0f;
		dl->AddText(ImVec2(base.x, y), col, "0");
		dl->AddText(ImVec2(base.x + w * 0.5f - 4.f, y), col, "0.5");
		dl->AddText(ImVec2(base.x + w - 8.f, y), col, "1");
	}
	ImGui::Spacing();

	// ヒント（コンパクト・1行）
	ImGui::TextDisabled("右クリック:追加  Ctrl+クリック:削除  ドラッグ:移動");
	ImGui::SameLine();
	// 点を手動追加ボタン（右クリックが効かない場合の代替）
	if (ImGui::SmallButton("+点")) {
		// 中央付近で近い点がなければ追加
		float midX = 0.5f;
		bool  tooClose = false;
		for (const auto& p : delegate_.points)
			if (std::abs(p.x - midX) < 0.05f) { tooClose = true; break; }
		if (!tooClose) {
			delegate_.AddPoint(0, ImVec2(midX, 1.0f));
			isDirty_ = true;
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("X=0.5 付近に制御点を追加");

	ImGui::Separator();

	// ============================================================
	// ボタン行
	// ============================================================

	// --- ランタイム適用 ---
	{
		const bool dis = !isDirty_;
		if (dis) ImGui::BeginDisabled();
		if (ImGui::Button("\uf0e7 適用##apply", ImVec2(80, 0))) {
			PushToMotion();
			isDirty_ = false;
			context_->statusMsg = "スピードカーブをランタイム適用しました";
		}
		if (dis) ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("カーブをMotionに反映します（キーフレームは変更しません）");
	}

	ImGui::SameLine(0, 6);

	// --- 焼き込み ---
	if (ImGui::Button("\uf0c7 Bake##bake", ImVec2(80, 0))) {
		bakeConfirmPending_ = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("カーブをキーフレーム時間軸に積分して焼き込みます\n（不可逆操作）");

	if (bakeConfirmPending_) {
		ImGui::OpenPopup("##BakeConfirm");
		bakeConfirmPending_ = false;
	}
	if (ImGui::BeginPopupModal("##BakeConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("カーブをキーフレームに焼き込みます。");
		ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "この操作は元に戻せません。");
		ImGui::Spacing();
		if (ImGui::Button("実行##bakeok", ImVec2(80, 0))) {
			// 焼き込み前にスナップショット保存
			bakedSnapshot_ = delegate_.points;
			hasBakedSnapshot_ = true;
			showBakedPreview_ = true;

			PushToMotion();
			BakeSpeedCurve();
			PullFromMotion();
			isDirty_ = false;
			selection_.clear();
			context_->statusMsg = "スピードカーブを焼き込みました";
			context_->requireTimelineRebuild = true;
			context_->lastAppliedScrubTime = -1.0f;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine(0, 8);
		if (ImGui::Button("キャンセル##bakecancel", ImVec2(80, 0)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SameLine(0, 6);

	// --- リセット ---
	if (ImGui::Button("\uf0e2 リセット##reset", ImVec2(80, 0))) {
		motion->ClearSpeedCurve();
		PullFromMotion();
		isDirty_ = false;
		hasBakedSnapshot_ = false;
		showBakedPreview_ = false;
		selection_.clear();
		context_->statusMsg = "スピードカーブをリセットしました";
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("カーブを等速（直線）に戻します");

	// ============================================================
	// 現在位置の速度プレビュー（コンパクト・右寄せ）
	// ============================================================
	if (motion->GetDuration() > 0.0f) {
		float nt = std::clamp(context_->scrubTime / motion->GetDuration(), 0.0f, 1.0f);
		float spd = motion->EvaluateSpeedCurve(nt);
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 140.0f);
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "現在: %.2fx  (t=%.2f)", spd, nt);
	}

	ImGui::PopID();
#endif
}

// -----------------------------------------------------------------------
// DrawBakedPreviewOverlay
// 焼き込み前スナップショットをオレンジ色の点線でオーバーレイ描画
// -----------------------------------------------------------------------
void SpeedCurvePanel::DrawBakedPreviewOverlay(ImVec2 rMin, ImVec2 rMax) const
{
#ifdef USE_IMGUI
	if (bakedSnapshot_.size() < 2) return;

	float w = rMax.x - rMin.x;
	float h = rMax.y - rMin.y;
	float yMin = delegate_.boundsMin.y;
	float yMax = delegate_.boundsMax.y;
	float yRange = yMax - yMin;
	if (yRange < 1e-6f || w < 1.0f || h < 1.0f) return;

	auto toScreen = [&](ImVec2 p) -> ImVec2 {
		return {
			rMin.x + std::clamp(p.x, 0.0f, 1.0f) * w,
			rMax.y - (std::clamp(p.y, yMin, yMax) - yMin) / yRange * h
		};
		};

	auto* dl = ImGui::GetWindowDrawList();
	const ImU32 lineCol = IM_COL32(255, 160, 40, 200);
	const ImU32 dotCol = IM_COL32(255, 200, 80, 220);

	// 折れ線
	for (int i = 0; i + 1 < static_cast<int>(bakedSnapshot_.size()); ++i) {
		dl->AddLine(toScreen(bakedSnapshot_[i]), toScreen(bakedSnapshot_[i + 1]),
			lineCol, 1.5f);
	}
	// 制御点マーカー（小さい円）
	for (const auto& p : bakedSnapshot_) {
		dl->AddCircleFilled(toScreen(p), 3.5f, dotCol);
	}

	// 凡例ラベル
	dl->AddRectFilled(ImVec2(rMin.x + 6, rMax.y - 20),
		ImVec2(rMin.x + 90, rMax.y - 6),
		IM_COL32(30, 30, 30, 180), 3.f);
	dl->AddText(ImVec2(rMin.x + 8, rMax.y - 18), dotCol, "Bake\xe5\x89\x8d"); // "Bake前"
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
		delegate_.points = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
	}
	else {
		for (const auto& kf : kfs)
			delegate_.points.push_back({ kf.time, kf.value });
		delegate_.Sort();
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
	for (const auto& p : delegate_.points)
		kfs.push_back({ p.x, p.y });

	std::sort(kfs.begin(), kfs.end(),
		[](const Motion::Keyframe<float>& a, const Motion::Keyframe<float>& b) {
			return a.time < b.time;
		});
#endif
}

// -----------------------------------------------------------------------
// BakeSpeedCurve
// -----------------------------------------------------------------------
//
// 【アルゴリズム】
//   速度カーブ s(t) (t=正規化アニメ時間) がある場合、
//   実時間 τ とアニメ時間 A の関係は:
//       dA/dτ = s(A/dur)
//   → τ(A) = ∫₀^A  1/s(a/dur) da   (実時間への変換)
//
//   焼き込み後のキーフレーム時刻 = τ(元の時刻)
//   焼き込み後の duration        = τ(元のduration)
//
// -----------------------------------------------------------------------
void SpeedCurvePanel::BakeSpeedCurve()
{
	Motion* motion = context_ ? context_->currentMotion : nullptr;
	if (!motion || !motion->HasSpeedCurve()) return;

	const float duration = motion->GetDuration();
	if (duration <= 0.0f) return;

	// ----------------------------------------------------------------
	// 1/s(t) の台形積分 LUT  (単位: 元のduration基準の秒)
	// invLut[i] = ∫₀^{i/N} (1/s(u)) du * duration = アニメ時間 i/N*duration に到達するまでの実時間
	// ----------------------------------------------------------------
	constexpr int kSamples = 512;
	constexpr float kMinSpeed = 0.05f;  // ゼロ割り防止の最低速度

	std::vector<float> invLut(kSamples + 1);
	invLut[0] = 0.0f;
	for (int i = 1; i <= kSamples; ++i) {
		float t0 = static_cast<float>(i - 1) / kSamples;
		float t1 = static_cast<float>(i) / kSamples;
		float s0 = std::max(motion->EvaluateSpeedCurve(t0), kMinSpeed);
		float s1 = std::max(motion->EvaluateSpeedCurve(t1), kMinSpeed);
		// 台形則: dt = (1/s0 + 1/s1) * 0.5 * (区間幅)
		invLut[i] = invLut[i - 1] + (1.0f / s0 + 1.0f / s1) * 0.5f / kSamples;
	}

	// invLut の値は「正規化された実時間」 → duration 倍で秒に戻す
	const float newDurationNorm = invLut[kSamples];  // 正規化新duration
	if (newDurationNorm < 1e-6f) return;

	const float newDuration = newDurationNorm * duration;

	// ----------------------------------------------------------------
	// キーフレーム時刻を直接リマップ
	// 元の時刻 T → 新時刻 T' = invLut[T/duration * N] * duration
	// (逆引きではなく直接参照)
	// ----------------------------------------------------------------
	auto remapTime = [&](float T) -> float {
		// LUTインデックスに変換（線形補間）
		float normT = std::clamp(T / duration, 0.0f, 1.0f);
		float fIdx = normT * static_cast<float>(kSamples);
		int   lo = static_cast<int>(fIdx);
		int   hi = std::min(lo + 1, kSamples);
		float frac = fIdx - static_cast<float>(lo);

		float newNormT = invLut[lo] + frac * (invLut[hi] - invLut[lo]);
		return newNormT * duration;  // 元のdurationスケールで返す（後でduration更新）
		};

	for (auto& [boneName, nodeAnim] : motion->animation_.nodeAnimations_) {
		auto remap = [&](auto& track) {
			for (auto& kf : track.keyframes) {
				kf.time = remapTime(kf.time);
			}
			};
		remap(nodeAnim.translate);
		remap(nodeAnim.rotate);
		remap(nodeAnim.scale);
	}

	// ----------------------------------------------------------------
	// duration を実時間に更新 & カーブをクリア
	// ----------------------------------------------------------------
	motion->SetDuration(newDuration);
	motion->ClearSpeedCurve();
}
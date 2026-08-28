#include "GuardEditor.h"

#ifdef USE_IMGUI

#include "PlayerGuard.h"

#include <imgui.h>

#include <algorithm>

namespace {

// トラックの並び順。実行順と同じにしておくと読み違えにくい。
enum TrackIndex {
	kTrackStartup = 0, // 発生（まだ防げない）
	kTrackActive,      // 防御が成立している区間
	kTrackParry,       // パリィ受付区間（防御区間の内側）
	kTrackRecovery,    // 解除後の硬直
	kTrackCount,
};

// 区間バーを1本だけ持つトラックを作る
DopeSheet::DopeTrack MakeSpanTrack(const char *label, DopeSheet::Color color, int beginFrame,
                                   int lengthFrames) {
	using namespace DopeSheet;

	DopeTrack track;
	track.label = label;
	track.type = TrackType::Generic;
	track.color = color;

	// duration は「区間の長さ-1」。長さ0の区間は表示しない。
	if (lengthFrames > 0) {
		DopeKey key(beginFrame, 0.0f, 0, std::max(0, lengthFrames - 1));
		key.shape = KeyShape::Bar;
		track.AddKey(key);
	}
	return track;
}

// トラックの先頭キーから「開始フレーム」と「長さ」を読み出す
bool ReadSpan(const DopeSheet::DopeTrack &track, int &outBegin, int &outLength) {
	if (track.keys.empty()) {
		outBegin = 0;
		outLength = 0;
		return false;
	}
	const auto &key = track.keys.front();
	outBegin = key.frame;
	outLength = key.duration + 1;
	return true;
}

} // namespace

// ============================================================
// タイムライン表示用のトラックを組み立てる
// ============================================================
void GuardEditor::BuildTracks(const GuardTimeline &timeline) {
	using namespace DopeSheet;

	tracks_.clear();
	tracks_.reserve(kTrackCount);

	// 発生：押してから防御が成立するまで。灰色で「まだ守れない」ことを示す。
	tracks_.push_back(MakeSpanTrack("発生", Color::Gray(), 0, timeline.startupFrames));

	// 防御中：ここに当たれば最低でも通常ガードになる。
	tracks_.push_back(
		MakeSpanTrack("防御中", Color::Blue(), timeline.ActiveBeginFrame(), timeline.activeFrames));

	// パリィ：防御区間の内側にある短い区間。
	const int parryLength = timeline.parryEndFrame - timeline.parryStartFrame + 1;
	tracks_.push_back(
		MakeSpanTrack("パリィ", Color::Yellow(), timeline.ParryBeginFrame(), parryLength));

	// 硬直：解除後に動けない時間。
	const int recoveryBegin = timeline.startupFrames + timeline.activeFrames;
	tracks_.push_back(
		MakeSpanTrack("硬直", Color::Orange(), recoveryBegin, timeline.recoveryFrames));

	cachedTimeline_ = timeline;
	hasCache_ = true;
}

// ============================================================
// ドープシートの編集結果を設定へ書き戻す
//
// バーの「開始位置」と「長さ」だけを見て復元する。
// 各区間は前の区間の終わりから始まる決まりなので、
// 発生と防御の長さが決まればパリィと硬直の位置も定まる。
// ============================================================
void GuardEditor::ApplyTracks(GuardTimeline &timeline) {
	int begin = 0;
	int length = 0;

	// 発生：長さのみ使う（必ず0フレーム目から始まる）
	if (ReadSpan(tracks_[kTrackStartup], begin, length)) {
		timeline.startupFrames = length;
	} else {
		timeline.startupFrames = 0;
	}

	// 防御中：長さのみ使う
	if (ReadSpan(tracks_[kTrackActive], begin, length)) {
		timeline.activeFrames = length;
	}

	// パリィ：防御区間の先頭からの相対位置に直す
	if (ReadSpan(tracks_[kTrackParry], begin, length)) {
		timeline.parryStartFrame = begin - timeline.ActiveBeginFrame();
		timeline.parryEndFrame = timeline.parryStartFrame + length - 1;
	}

	// 硬直：長さのみ使う
	if (ReadSpan(tracks_[kTrackRecovery], begin, length)) {
		timeline.recoveryFrames = length;
	} else {
		timeline.recoveryFrames = 0;
	}

	timeline.Sanitize();
}

// ============================================================
// 現在の設定がどういうガードになるかの要約
// 数値を見ただけでは気づきにくい破綻をここで警告する
// ============================================================
void GuardEditor::DrawSummary(const GuardConfig &config) const {
	const GuardTimeline &tl = config.timeline;
	const float fps = static_cast<float>(std::max(1, tl.fps));
	const int parryLength = tl.parryEndFrame - tl.parryStartFrame + 1;

	ImGui::Text("防御が成立するのは %d〜%d フレーム (%.2f秒間)", tl.ActiveBeginFrame(),
	            tl.ActiveBeginFrame() + tl.activeFrames - 1, tl.activeFrames / fps);
	ImGui::Text("うちパリィは %d フレーム (%.2f秒)", parryLength, parryLength / fps);

	// パリィが持続をほぼ埋めていると「ガードすれば必ずパリィ」になり読み合いが消える
	if (parryLength >= tl.activeFrames) {
		ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f},
		                   "防御区間が全部パリィです。ガードすれば必ずパリィになります");
	} else if (parryLength > tl.activeFrames / 2) {
		ImGui::TextColored({1.0f, 0.8f, 0.3f, 1.0f},
		                   "パリィ区間が広めです。狙わなくても成立しやすくなります");
	}

	// 通常ガードが完全無効だとパリィを狙う理由がなくなる
	if (config.guard.damageRate <= 0.0f) {
		ImGui::TextColored({1.0f, 0.8f, 0.3f, 1.0f},
		                   "通常ガードでダメージ0です。パリィを狙う理由が薄くなります");
	}
}

// ============================================================
// 結果パラメータの編集
// ============================================================
void GuardEditor::DrawOutcome(const char *label, GuardOutcome &outcome, const char *idSuffix) {
	if (!ImGui::CollapsingHeader(label)) return;

	ImGui::PushID(idSuffix);

	ImGui::SeparatorText("ゲーム的な結果");
	ImGui::SliderFloat("通すダメージ割合", &outcome.damageRate, 0.0f, 1.0f, "%.2f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("0 = 完全無効 / 1 = 素通り。\n通常ガードを0にするとパリィの価値が無くなる。");
		ImGui::EndTooltip();
	}
	ImGui::DragInt("消費CC", &outcome.ccCost, 1, 0, 10);
	ImGui::DragInt("回復CC", &outcome.ccRecover, 1, 0, 10);

	ImGui::SeparatorText("手応え");
	ImGui::DragFloat("ヒットストップ", &outcome.hitStop, 0.01f, 0.0f, 1.0f, "%.2f秒");
	ImGui::DragFloat("停止明けの復帰", &outcome.hitStopEase, 0.01f, 0.0f, 1.0f, "%.2f秒");
	ImGui::DragFloat("カメラ揺れ強さ", &outcome.shakeIntensity, 0.01f, 0.0f, 3.0f, "%.2f");
	ImGui::DragFloat("カメラ揺れ時間", &outcome.shakeDuration, 0.01f, 0.0f, 1.0f, "%.2f秒");

	ImGui::SeparatorText("押し合い（どちらが動くか）");
	ImGui::DragFloat("自分が下がる距離", &outcome.selfPushDistance, 0.05f, 0.0f, 5.0f, "%.2fm");
	ImGui::DragFloat("自分が下がる時間", &outcome.selfPushDuration, 0.01f, 0.0f, 1.0f, "%.2f秒");
	ImGui::DragFloat("相手を押す強さ", &outcome.enemyPushPower, 0.5f, 0.0f, 40.0f, "%.1f");
	ImGui::DragFloat("相手を押す時間", &outcome.enemyPushDuration, 0.01f, 0.0f, 1.0f, "%.2f秒");
	ImGui::TextDisabled("通常ガードは自分が下がり、パリィは相手を押す形にすると差が出ます");

	ImGui::SeparatorText("盾のスケール変化");
	ImGui::DragFloat("変化量", &outcome.shieldSquash, 0.01f, 0.0f, 2.0f, "%.2f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("0.4〜0.6 くらいではっきり見える。\n小さいと画面上ではほぼ分からない。");
		ImGui::EndTooltip();
	}
	ImGui::DragFloat("戻る時間", &outcome.shieldSquashTime, 0.01f, 0.02f, 1.0f, "%.2f秒");
	ImGui::DragFloat3("軸ごとの効き", &outcome.shieldSquashAxis.x, 0.05f, -2.0f, 2.0f, "%.2f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("正で伸び、負で縮む。\n"
		                       "(0.6, 0.6, -1.0) = 面方向に広がり厚みが潰れる（受け止めた形）\n"
		                       "(1, 1, 1)        = 単純に大きくなる（漫画的なポップ）");
		ImGui::EndTooltip();
	}
	ImGui::DragFloat("跳ね返り", &outcome.shieldSquashBounce, 0.05f, 0.5f, 4.0f, "%.2f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("波の回数。1.0で単純な山、1.5以上で戻りぎわに逆向きの振れが入る。");
		ImGui::EndTooltip();
	}

	ImGui::SeparatorText("エフェクト");
	char vfxBuffer[128];
	std::snprintf(vfxBuffer, sizeof(vfxBuffer), "%s", outcome.vfxName.c_str());
	if (ImGui::InputText("Composite名", vfxBuffer, sizeof(vfxBuffer))) {
		outcome.vfxName = vfxBuffer;
	}

	ImGui::PopID();
}

// ============================================================
// エディタ本体
// ============================================================
void GuardEditor::Draw() {
	if (!guard_) {
		ImGui::TextDisabled("プレイヤーが未設定です");
		return;
	}

	GuardConfig &config = guard_->GetConfig();
	GuardTimeline &timeline = config.timeline;

	// ------------------------------------------------------------
	// 外部（コードや読み込み）でタイムラインが変わったらトラックを組み直す
	// ------------------------------------------------------------
	const bool timelineChangedOutside =
		!hasCache_ || cachedTimeline_.startupFrames != timeline.startupFrames ||
		cachedTimeline_.activeFrames != timeline.activeFrames ||
		cachedTimeline_.parryStartFrame != timeline.parryStartFrame ||
		cachedTimeline_.parryEndFrame != timeline.parryEndFrame ||
		cachedTimeline_.recoveryFrames != timeline.recoveryFrames;

	if (timelineChangedOutside) {
		BuildTracks(timeline);
	}

	// ------------------------------------------------------------
	// タイムライン
	// ------------------------------------------------------------
	ImGui::SeparatorText("タイムライン（バーの端をドラッグして長さを変更）");

	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputInt("FPS", &timeline.fps);
	timeline.fps = std::max(1, timeline.fps);

	// 実行中の進行位置を再生ヘッドとして出す
	const int liveFrame = guard_->GetTimelineFrame();
	if (liveFrame >= 0) {
		dopeEditor_.SetSeekFrame(liveFrame);
	}

	const int totalFrames = std::max(1, timeline.TotalFrames());
	if (dopeEditor_.Draw("##guard_dope", tracks_, totalFrames, timeline.fps)) {
		ApplyTracks(timeline);
		BuildTracks(timeline); // 整合を取った結果を表示へ戻す
	}

	ImGui::Separator();
	DrawSummary(config);

	// ------------------------------------------------------------
	// 結果パラメータ
	// ------------------------------------------------------------
	ImGui::SeparatorText("成立したときの結果");
	DrawOutcome("通常ガード（受け止めて押される）", config.guard, "guard");
	DrawOutcome("パリィ（弾いて相手を突き放す）", config.parry, "parry");

	ImGui::SeparatorText("その他");
	ImGui::DragFloat("防げる角度", &config.frontHalfAngleDeg, 1.0f, 10.0f, 180.0f, "正面から±%.0f度");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("この角度の外から来た攻撃は防げない。\n180にすると背後からの攻撃も防げてしまう。");
		ImGui::EndTooltip();
	}

	// ------------------------------------------------------------
	// 保存・読み込み
	// ------------------------------------------------------------
	ImGui::Separator();
	if (ImGui::Button("JSONに保存")) {
		guard_->SaveConfig(filePath_);
	}
	ImGui::SameLine();
	if (ImGui::Button("JSONから再読み込み")) {
		guard_->LoadConfig(filePath_);
		hasCache_ = false; // トラックを作り直させる
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", filePath_.c_str());

	// ------------------------------------------------------------
	// 実行中の状態
	// ------------------------------------------------------------
	ImGui::SeparatorText("実行中の状態");
	guard_->ShowDebugImGui();
}

#endif // USE_IMGUI

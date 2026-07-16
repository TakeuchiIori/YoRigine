#include "MissionUI.h"

#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>
#include <Systems/Text/TextTextureBaker.h>
#include <Systems/GameTime/GameTime.h>
#include "Trigger/WaypointManager.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
	// 演出の時間（秒）
	constexpr float kWinFade   = 0.4f;   // ウィンドウのフェードイン
	constexpr float kCheckPop  = 0.45f;  // Check のポップ
	constexpr float kCheckHold = 1.6f;   // Check を出しておく時間
	constexpr float kMissionFade = 0.45f; // 次のミッション文のフェードイン

	// ミッションUIの描画レイヤー（ControlUI=1 / GameUI=0,2 とかぶらない層）
	constexpr int kLayer = 3;

	const std::string kTexDir    = "Resources/Textures/Operation/";
	const std::string kBakeDir   = "Resources/Textures/Mission/";
	const std::string kConfigDir = "Resources/UIConfigs/GameScene/";
	const std::string kFont      = "Resources/Fonts/ipaexg.ttf"; // 黒薔薇より読みやすい日本語ゴシック
}

// ============================================================
// 初期化
// ============================================================
void MissionUI::Initialize()
{
	// 背景・枠・目標文・チェック。位置/サイズは UIConfig（UIEditor）で調整できる。
	// テクスチャ実寸(MissionWindow/Check=512px)のまま出すと巨大になるので既定サイズを明示指定。
	// text_ は runtime でベイク画像に貼り替えるため、初期テクスチャは実在する背景を仮置き。
	window_ = GetOrCreate("MissionWindow", kTexDir + "MissionWindow.png", { 320.0f, 130.0f }, { 440.0f, 130.0f }, { 0.5f, 0.5f }, kLayer);
	frame_  = GetOrCreate("MissionFrame",  kTexDir + "frame_square.png",  { 320.0f, 130.0f }, { 440.0f, 130.0f }, { 0.5f, 0.5f }, kLayer);
	text_   = GetOrCreate("MissionText",   kTexDir + "MissionWindow.png", { 120.0f, 130.0f }, {  40.0f,  40.0f }, { 0.0f, 0.5f }, kLayer);
	check_  = GetOrCreate("MissionCheck",  kTexDir + "Check.png",         { 500.0f, 130.0f }, {  72.0f,  72.0f }, { 0.5f, 0.5f }, kLayer);

	// 表示時に焼き直す基準カラーを config から取り込む。アルファは [0,1] にクランプして
	// ゴミ値（実行時に色バッファが化けるケース）を持ち込まないようにする。
	auto sane = [](Vector4 c) {
		auto channel = [](float value) {
			return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
		};
		c.x = channel(c.x);
		c.y = channel(c.y);
		c.z = channel(c.z);
		c.w = channel(c.w);
		return c;
	};
	if (window_) windowColor_ = sane(window_->GetColor());
	if (frame_)  frameColor_  = sane(frame_->GetColor());
	if (text_)   textColor_   = sane(text_->GetColor());

	HideWindow();
}

// ============================================================
// 更新（WaypointManager をポーリングして表示を駆動）
// ============================================================
void MissionUI::Update()
{
	WaypointManager* wm = WaypointManager::GetInstance();
	bool fadeInNextMission = false;

	// クリア時は WaypointManager がすでに次の目標へ切り替わっていても、
	// チェック演出が終わるまで現在の文言を保持する。
	if (completionPending_) {
		if (!fieldActive_) {
			HideWindow();
			return;
		}

		if (window_) { window_->SetVisible(true); window_->SetColor(windowColor_); }
		if (frame_)  { frame_->SetVisible(true);  frame_->SetColor(frameColor_); }
		if (text_)   { text_->SetVisible(true);   text_->SetColor(textColor_); }
		windowShown_ = true;

		checkTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();
		if (checkTimer_ < kCheckHold) {
			return;
		}

		completionPending_ = false;
		checkVisible_ = false;
		if (check_) check_->SetVisible(false);
		fadeInNextMission = true;
	}

	// フィールドシーンで、かつ現在の目標がある時だけ表示する。
	// フィールドシーンで、かつ現在の目標がある間は「常に」表示する（シリアル変化の瞬間だけに
	// 頼らず状態ベースで維持）。これで開始直後の startActive 目標も確実に出る。
	const bool wantShow = fieldActive_ && wm->HasCurrent();

	if (wantShow) {
		const uint32_t serial = wm->GetMissionSerial();
		if (serial != shownSerial_) {
			// 新しい目標に切り替わった → 目標文を焼き直す。
			// 起動時から読めるよう、テキストはフェード開始で
			// alpha=0 にせず有効な作者カラーを即時反映する。
			shownSerial_  = serial;
			currentTitle_ = wm->GetCurrentMissionTitle();
			reqCount_     = wm->GetCurrentRequiredCount();
			curCount_     = wm->GetCurrentProgress();
			RebakeText();
			if (text_) {
				text_->StopAnimation(UIAnimationType::Alpha);
				text_->SetColor(textColor_);
				if (fadeInNextMission) {
					text_->PlayAlphaAnimation(
						0.0f, textColor_.w, kMissionFade,
						Easing::Function::EaseOutQuad, false);
				}
			}
		}
		else {
			// 同じ目標：撃破数が進んだらカウンタを更新
			const int p = wm->GetCurrentProgress();
			if (p != curCount_) {
				curCount_ = p;
				RebakeText();
				if (text_) text_->PlayFlash(0.25f, 3);
			}
		}

		// 背景・枠・目標文を毎フレーム可視化し、window/frame は config カラーで塗り直す。
		// 表示タイミングの取りこぼしと、実行時のアルファ化けの両方をこれで防ぐ。
		if (window_) { window_->SetVisible(true); window_->SetColor(windowColor_); }
		if (frame_)  { frame_->SetVisible(true);  frame_->SetColor(frameColor_); }
		if (text_)   { text_->SetVisible(true); text_->SetColor(textColor_); }
		windowShown_ = true;
	}
	else {
		// バトル中 or 目標なし → 隠す
		HideWindow();
	}

}

// ============================================================
// 描画（自分のレイヤーだけ描く）
// ============================================================
void MissionUI::Draw()
{
	auto layer = YoRigine::UIManager::GetInstance()->GetUIsByLayer(kLayer);
	for (auto& ui : layer) {
		ui->Draw();
	}
}

// ============================================================
// 通知: 目標クリア（Check をポップ表示）
// ============================================================
void MissionUI::OnMissionCleared()
{
	// フィールドで表示中のときだけ演出する（バトル中の撃破では出さない）。
	if (!fieldActive_ || !windowShown_) return;

	// カウンターも達成状態にしてからチェックを出す。
	curCount_ = reqCount_;
	RebakeText();

	completionPending_ = true;
	checkVisible_ = true;
	checkTimer_   = 0.0f;
	if (check_) {
		check_->SetVisible(true);
		check_->PlayScaleAnimation({ 0.1f, 0.1f }, { 1.0f, 1.0f }, kCheckPop, Easing::Function::EaseOutBack, false);
		check_->PlayFlash(kCheckPop, 6);
	}
}

// ============================================================
// 目標文の焼き直し
// ============================================================
void MissionUI::RebakeText()
{
	if (!text_) return;
	const std::string path = BakeMissionText(currentTitle_, curCount_, reqCount_);
	if (!path.empty()) {
		text_->SetTexture(path); // 実寸に更新（左アンカーなので左詰めで伸びる）
	}
}

std::string MissionUI::BakeMissionText(const std::string& title, int cur, int req)
{
	std::error_code ec;
	std::filesystem::create_directories(kBakeDir, ec);

	std::string label = title.empty() ? std::string("敵を倒せ！") : title;
	char counter[32];
	std::snprintf(counter, sizeof(counter), "  %d/%d", cur, req);
	label += counter;

	YoRigine::TextBakeParams params;
	params.text         = label;
	params.fontFilePath = kFont;
	params.fontSize     = 44.0f;
	params.fillColor    = { 1.0f, 1.0f, 1.0f, 1.0f };
	params.outlineWidth = 5.0f;
	params.outlineColor = { 0.05f, 0.05f, 0.08f, 1.0f };
	params.padding      = 12.0f;

	// 連番パスにすることでテクスチャキャッシュを確実に回避（同名だと更新が反映されない）。
	const std::string path = kBakeDir + "mission_" + std::to_string(bakeSeq_++) + ".png";
	if (!YoRigine::TextTextureBaker::Bake(params, path)) {
		return "";
	}
	return path;
}

// ============================================================
// ウィンドウ（背景＋枠）の表示・非表示
// ============================================================
void MissionUI::HideWindow()
{
	windowShown_ = false;
	shownSerial_ = 0; // 次に表示されるとき必ず「新目標」として焼き直す
	completionPending_ = false;
	checkVisible_ = false;
	if (window_) window_->SetVisible(false);
	if (frame_)  frame_->SetVisible(false);
	if (text_)   text_->SetVisible(false);
	if (check_)  check_->SetVisible(false);
}

// ============================================================
// UI取得 or 生成（ControlUI の GetOrCreateButton と同じ流儀 + anchor/layer 指定）
// ============================================================
UIBase* MissionUI::GetOrCreate(const std::string& id, const std::string& texturePath,
	const Vector2& pos, const Vector2& size, const Vector2& anchor, int layer)
{
	auto* mgr = YoRigine::UIManager::GetInstance();
	if (UIBase* exist = mgr->GetUI(id)) return exist;

	const std::string cfg = kConfigDir + id + ".json";
	const bool hadCfg = std::filesystem::exists(cfg);

	auto ui = std::make_unique<UIBase>(id);
	ui->Initialize(cfg); // 既存あれば位置/サイズ/テクスチャを復元、無ければ configPath を設定

	if (!hadCfg) {
		ui->SetTexture(texturePath);
		if (size.x > 0.0f && size.y > 0.0f) ui->SetSize(size); // 0 はテクスチャ実寸
		ui->SetAnchorPoint(anchor);
		ui->SetLayer(layer);
		ui->SetPosition({ pos.x, pos.y, 0.0f });
		ui->SaveToJSON();
	}

	UIBase* raw = ui.get();
	mgr->AddUI(id, std::move(ui));
	return raw;
}

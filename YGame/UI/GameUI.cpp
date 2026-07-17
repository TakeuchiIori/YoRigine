#include "GameUI.h"
#include "Mission/MissionUI.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/Input/Input.h>
#include <Systems/GameTime/GameTime.h>
#include <Systems/Cinematic/CinematicManager.h>
#include <Systems/Text/TextTextureBaker.h>
#include "OffScreen/PostEffectManager.h"
#include "Trigger/WaypointManager.h"
#include <filesystem>
#include <iterator>

// ControlUI / MissionUI を不完全型のまま unique_ptr メンバに持つため、
// 実体が見える .cpp 側でコンストラクタ／デストラクタを定義する。
GameUI::GameUI() = default;
GameUI::~GameUI() {
	// シングルトンの WaypointManager に登録したコールバックが this（破棄済み）を
	// 指し続けないよう、破棄時に必ず外す。
	WaypointManager::GetInstance()->ClearMissionCallbacks();
}

// ============================================================
// ポーズメニューのレイアウト定数（マジックナンバーを排除）
//   座標系は画面ピクセル(1600x900・アンカー中心)。色は RGBA 0..1。
// ============================================================
namespace PauseLayout {
	// 色
	const Vector4 kGold     { 0.85f, 0.72f, 0.25f, 1.0f };  // タイトル/枠
	const Vector4 kOlive    { 0.43f, 0.40f, 0.19f, 1.0f };  // 行ラベル
	const Vector4 kWhite    { 1.0f, 1.0f, 1.0f, 1.0f };
	const Vector4 kPanel    { 0.04f, 0.05f, 0.08f, 0.72f };  // パネル地
	const Vector4 kDim      { 0.0f, 0.0f, 0.0f, 0.7f };      // 背景暗転
	const Vector4 kHighlight{ 0.85f, 0.72f, 0.25f, 0.30f };  // 選択バー

	// 画面・パネル共通
	constexpr float kScreenW = 1600.0f, kScreenH = 900.0f;
	constexpr float kPanelY = 500.0f;
	constexpr float kFrameRadius = 22.0f, kFrameThickness = 5.0f;

	// 左パネル（メニュー）
	constexpr float kLeftX = 400.0f, kLeftW = 600.0f, kLeftH = 640.0f;
	constexpr float kPauseTitleY = 300.0f;
	constexpr float kResumeY = 500.0f, kTitleItemY = 630.0f;  // メニュー項目のY
	constexpr float kHighlightW = 540.0f, kHighlightH = 124.0f;

	// 右パネル（操作一覧）
	constexpr float kRightX = 1140.0f, kRightW = 860.0f, kRightH = 720.0f;
	constexpr float kControlsTitleY = 230.0f;
	constexpr float kRowStartY = 350.0f, kRowStepY = 100.0f;  // 1行目のYと行間
	constexpr float kIconX = 830.0f, kLabelX = 1240.0f;
	constexpr float kIconSize = 80.0f, kShoulderIconSize = 64.0f;
	constexpr float kShoulderOffset = 36.0f;  // LB/RB を中心から左右にずらす量
	constexpr float kCamLabelOffset = 30.0f;  // カメラリセットラベルの右寄せ微調整

	// フォントサイズ / 字間（負＝詰める）
	constexpr float kPauseTitleSize = 140.0f, kControlsTitleSize = 84.0f;
	constexpr float kRowFontSize = 80.0f, kMenuFontSize = 72.0f;
	constexpr float kRowSpacing = -3.0f, kTitleSpacing = -2.0f;
}

/// <summary>
/// 初期化処理
/// </summary>
void GameUI::Initialize()
{
	YoRigine::UIManager::GetInstance()->LoadScene("GameScene");

	// ゲームオーバーUI取得
	gameOver_ = YoRigine::UIManager::GetInstance()->GetUI("GameOver");
	background_ = YoRigine::UIManager::GetInstance()->GetUI("BackGround");
	retryButton_ = YoRigine::UIManager::GetInstance()->GetUI("Retry");
	titleButton_ = YoRigine::UIManager::GetInstance()->GetUI("ToTitle");

	// 操作用UI取得
	padA_ = YoRigine::UIManager::GetInstance()->GetUI("A");
	padB_ = YoRigine::UIManager::GetInstance()->GetUI("B");
	padLB_ = YoRigine::UIManager::GetInstance()->GetUI("LB");
	strong_ = YoRigine::UIManager::GetInstance()->GetUI("Strong");
	weak_ = YoRigine::UIManager::GetInstance()->GetUI("Weak");
	guard_ = YoRigine::UIManager::GetInstance()->GetUI("Guard");
	uiBackGround_ = YoRigine::UIManager::GetInstance()->GetUI("UIBackGround");
	startButton_ = YoRigine::UIManager::GetInstance()->GetUI("startButton");
	toTitleButton_ = YoRigine::UIManager::GetInstance()->GetUI("title");
	toGameButton_ = YoRigine::UIManager::GetInstance()->GetUI("toGame");

	// ポーズメニューを2パネルレイアウトで構築（左メニュー＋右操作一覧）。
	BuildPauseLayout();

	// ポーズメニュー用の元サイズ・色情報保存用マップ初期化
	pauseOriginalScales_.clear();
	pauseOriginalColors_.clear();

	// ゲームオーバーUIを初期状態で非表示
	if (gameOver_) gameOver_->SetVisible(false);
	if (background_) background_->SetVisible(false);
	if (retryButton_) {
		retryButton_->SetVisible(false);
		// 元のサイズを保存
		retryButtonOriginalSize_ = retryButton_->GetScale();
	}
	if (titleButton_) {
		titleButton_->SetVisible(false);
		// 元のサイズを保存
		titleButtonOriginalSize_ = titleButton_->GetScale();
	}

	// ポーズメニュー用ボタンの元サイズ保存
	if (toGameButton_) {
		toGameOriginalSize_ = toGameButton_->GetScale();
	}
	if (toTitleButton_) {
		toTitleOriginalSize_ = toTitleButton_->GetScale();
	}

	// 念のため初期状態をクリア
	ResetGameOver();

	// ★ 初期状態の設定
	pauseState_ = PauseState::HIDDEN;

	// レイヤー2（ポーズUI）の作者値（JSON から読まれたスケール/色）を控えてから、
	// 「隠れ状態」へ戻す。ここで scale0/alpha0 を live UI に焼いてしまうと、その状態で
	// UI 保存された時に 0 が JSON に書かれ、二度とポーズメニューが出なくなる。
	// → 隠れ状態は visible=false のみとし、スケール/色は作者値を維持する。
	auto layer2 = YoRigine::UIManager::GetInstance()->GetUIsByLayer(2);
	for (auto& ui : layer2) {
		pauseOriginalScales_[ui] = ui->GetScale();
		pauseOriginalColors_[ui] = ui->GetColor();
	}
	RestorePauseRestingState();

	controlUI_ = std::make_unique<ControlUI>();
	controlUI_->Initialize();

	// ミッションUI（ウェイポイント進行の目標表示）。WaypointManager からの
	// 通知をこのUIへ中継する。ラムダは this を捕らえるので ~GameUI で必ず外す。
	missionUI_ = std::make_unique<MissionUI>();
	missionUI_->Initialize();
	// 目標文・進捗・表示可否は MissionUI が WaypointManager を毎フレーム参照して駆動する。
	// クリアの瞬間だけ Check をポップさせたいので、そのイベントだけコールバックで受ける。
	WaypointManager::GetInstance()->SetOnMissionCleared([this]() {
		if (missionUI_) missionUI_->OnMissionCleared();
		});
}

/// <summary>
/// 更新処理
/// </summary>
void GameUI::Update()
{
	// ■■■ 1. ポーズメニューのステート管理 ■■■

		// スタートボタン入力
	if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::Start)) {
		if (!goVisible_) { // ゲームオーバー中でなければ
			if (pauseState_ == PauseState::HIDDEN || pauseState_ == PauseState::CLOSING) {
				// --- 開く ---
				pauseState_ = PauseState::OPENING;
				pauseAnimTimer_ = 0.0f;
				YoRigine::GameTime::Pause(); // ゲーム時間を止める

				// 背景をぼかす（ポーズ中だけ GaussSmoothing を後段に追加）
				if (pauseBlurIndex_ < 0) {
					pauseBlurIndex_ = PostEffectManager::GetInstance()->AddEffect(
						OffScreen::OffScreenEffectType::GaussSmoothing, "PauseBlur");
				}

				// とりあえず表示フラグはONにする（サイズは0からスタート）
				for (auto& pair : pauseOriginalScales_) {
					if (pair.first) pair.first->SetVisible(true);
				}
			}
			else if (pauseState_ == PauseState::ACTIVE || pauseState_ == PauseState::OPENING) {
				// --- 閉じる ---
				pauseState_ = PauseState::CLOSING;
				pauseAnimTimer_ = 0.0f;
			}
		}
	}

	// アニメーション進行処理 (OPENING または CLOSING)
	if (pauseState_ == PauseState::OPENING || pauseState_ == PauseState::CLOSING) {
		// ゲーム停止中でも動く時間を使う
		pauseAnimTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();

		float t = pauseAnimTimer_ / pauseAnimDuration_;
		if (t >= 1.0f) t = 1.0f;

		if (pauseState_ == PauseState::OPENING) {
			// 開く: EaseOutBack (ボヨヨン)
			float easedT = Easing::EaseOutBack(t);
			UpdatePauseVisuals(easedT);

			if (t >= 1.0f) {
				pauseState_ = PauseState::ACTIVE;
			}
		}
		else if (pauseState_ == PauseState::CLOSING) {
			// 閉じる: 素直に縮小 (1.0 -> 0.0)
			// t は 0->1 と進むので、 (1 - t) を適用
			// よりリッチにするなら EaseOutCubic(1-t) など
			float closingT = 1.0f - std::pow(t, 3.0f); // 立方根的に縮む
			UpdatePauseVisuals(closingT);

			if (t >= 1.0f) {
				pauseState_ = PauseState::HIDDEN;
				YoRigine::GameTime::Resume(); // 時間を再開

				// ポーズ用ブラーを除去
				if (pauseBlurIndex_ >= 0) {
					PostEffectManager::GetInstance()->RemoveEffect(pauseBlurIndex_);
					pauseBlurIndex_ = -1;
				}

				// 完全に消えたら隠れ状態へ。閉じアニメで縮んだスケール/色を作者値へ戻し、
				// visible=false にする（live UI を保存安全な状態に保つ）。
				RestorePauseRestingState();
			}
		}
	}

	// ■■■ 2. ポーズ中の操作 (ACTIVE時のみ) ■■■
	if (pauseState_ == PauseState::ACTIVE) {
		// ステート遷移直後の漏れ防止のため念のためPause
		YoRigine::GameTime::Pause();

		// --- 選択操作（既存ロジック） ---
		float stickY = YoRigine::Input::GetInstance()->GetLeftStickY(0);
		const float stickThreshold = 0.5f;

		if (stickY > stickThreshold) currentPauseSelection_ = PauseSelection::TO_GAME;
		else if (stickY < -stickThreshold) currentPauseSelection_ = PauseSelection::TO_TITLE;

		// --- 決定ボタン ---
		if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
			if (currentPauseSelection_ == PauseSelection::TO_GAME) {
				pauseState_ = PauseState::CLOSING;
				pauseAnimTimer_ = 0.0f;
			}
			else {
				returnToTitleRequested_ = true;
			}
		}

		// --- 選択項目の拡大表示 ---
		// アニメーションが終わっているので、ここでは「選択されているボタン」だけ
		// baseScale * selectionScale_ に上書きする
		if (toGameButton_) {
			float scale = (currentPauseSelection_ == PauseSelection::TO_GAME) ? selectionScale_ : normalScale_;
			// 保存しておいた元サイズを使う
			Vector2 baseSize = pauseOriginalScales_[toGameButton_];
			toGameButton_->SetScale(baseSize * scale);
		}

		if (toTitleButton_) {
			float scale = (currentPauseSelection_ == PauseSelection::TO_TITLE) ? selectionScale_ : normalScale_;
			Vector2 baseSize = pauseOriginalScales_[toTitleButton_];
			toTitleButton_->SetScale(baseSize * scale);
		}

		// --- 選択ハイライトを選択中の項目へスッと移動 ---
		if (pauseSelectHighlight_) {
			float targetY = (currentPauseSelection_ == PauseSelection::TO_GAME)
				? PauseLayout::kResumeY : PauseLayout::kTitleItemY;
			Vector3 p = pauseSelectHighlight_->GetPosition();
			p.y += (targetY - p.y) * 0.35f;  // 簡易スムージング
			pauseSelectHighlight_->SetPosition(p);
		}
	}
	// 操作用UIの更新
	controlUI_->Update();
	if (missionUI_) missionUI_->Update();



	// UI全体の更新
	YoRigine::UIManager::GetInstance()->UpdateAll();

	// ゲームオーバーUIのフェード処理
	if (goFadingIn_) {
		goFadeTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();
		float t = (goFadeDuration_ > 0.0f) ? (goFadeTimer_ / goFadeDuration_) : 1.0f;

		// フェード完了判定
		if (t >= 1.0f) {
			t = 1.0f;
			goFadingIn_ = false;
			goFadeCompleted_ = true;

			// フェード完了後にボタンを表示し、初期スケールを設定
			if (retryButton_) {
				retryButton_->SetVisible(true);
				// 初期選択はリトライなので拡大表示
				retryButton_->SetScale(Vector2(retryButtonOriginalSize_.x * selectionScale_, retryButtonOriginalSize_.y * selectionScale_));
			}
			if (titleButton_) {
				titleButton_->SetVisible(true);
				// 非選択なので通常サイズ
				titleButton_->SetScale(Vector2(titleButtonOriginalSize_.x * normalScale_, titleButtonOriginalSize_.y * normalScale_));
			}
		}
		goAlpha_ = t;
	}

	// ゲームオーバー時のボタン入力チェック（フェード完了後のみ）
	if (goFadeCompleted_ && goVisible_) {
		// 左スティックで選択切り替え
		const float stickThreshold = 0.5f;
		static bool stickProcessed = false;

		float stickX = YoRigine::Input::GetInstance()->GetLeftStickX(0);

		if (abs(stickX) > stickThreshold) {
			if (!stickProcessed) {
				if (stickX > stickThreshold) {
					// 右に倒す → タイトルを選択
					currentSelection_ = GameOverSelection::TITLE;
				} else if (stickX < -stickThreshold) {
					// 左に倒す → リトライを選択
					currentSelection_ = GameOverSelection::RETRY;
				}
				stickProcessed = true;
			}
		} else {
			stickProcessed = false;
		}

		// スケールを選択状態に応じて更新
		if (retryButton_) {
			float scale = (currentSelection_ == GameOverSelection::RETRY) ? selectionScale_ : normalScale_;
			retryButton_->SetScale(Vector2(retryButtonOriginalSize_.x * scale, retryButtonOriginalSize_.y * scale));
		}
		if (titleButton_) {
			float scale = (currentSelection_ == GameOverSelection::TITLE) ? selectionScale_ : normalScale_;
			titleButton_->SetScale(Vector2(titleButtonOriginalSize_.x * scale, titleButtonOriginalSize_.y * scale));
		}

		// Aボタンで決定
		if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
			if (currentSelection_ == GameOverSelection::RETRY) {
				retryRequested_ = true;
			} else {
				returnToTitleRequested_ = true;
			}
		}

		// キーボード操作も維持（スペースでリトライ、ESCでタイトル）
		if (YoRigine::Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			currentSelection_ = GameOverSelection::RETRY;
			retryRequested_ = true;
		}
		if (YoRigine::Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
			currentSelection_ = GameOverSelection::TITLE;
			returnToTitleRequested_ = true;
		}
	}

	// フェード後のアルファ反映
	ApplyAlpha(goAlpha_);
}


/// <summary>
/// バトル中フラグを ControlUI へ伝える
/// </summary>
void GameUI::SetBattleActive(bool active)
{
	if (controlUI_) controlUI_->SetBattleActive(active);
	// ミッションUIはフィールドシーンでのみ表示（バトル中は隠す）。
	if (missionUI_) missionUI_->SetFieldActive(!active);
}

void GameUI::SetPlayerStyle(PlayerStyle style)
{
	if (controlUI_) controlUI_->SetPlayerStyle(style);
}

/// <summary>
/// ロックオン照準フラッシュを再生（ControlUI へ委譲）
/// </summary>
void GameUI::FlashLockOn()
{
	if (controlUI_) controlUI_->FlashLockOn();
}

void GameUI::SetLockOnFlashPos(const Vector3& screenPos)
{
	if (controlUI_) controlUI_->SetLockOnFlashPos(screenPos);
}

/// <summary>
/// UI描画（全部）
/// </summary>
void GameUI::DrawAll()
{
}

/// <summary>
/// UI描画（個別）
/// </summary>
void GameUI::Draw()
{

	auto layer = YoRigine::UIManager::GetInstance()->GetUIsByLayer(0);
	for (auto& ui : layer) {
		ui->Draw();
	}

	auto layer_2 = YoRigine::UIManager::GetInstance()->GetUIsByLayer(2);
	for (auto& ui : layer_2) {
		ui->Draw();
	}
	if (goVisible_)return;

	// ミッションはゲーム開始時の演出中でも表示する。
	// これを演出判定より後に置くと、startActive の目標が演出終了まで
	// 描画されない。
	if (missionUI_) missionUI_->Draw();

	// 演出中（クリアカットシーン等）は操作ヒントだけを隠す。
	if (!YoRigine::CinematicManager::GetInstance()->IsActive()) {
		controlUI_->Draw();
	}
}

/// <summary>
/// ゲームオーバーUIをフェード表示する
/// </summary>
/// <param name="duration">フェード時間（秒）</param>
void GameUI::ShowGameOverWithFade(float duration)
{
	goVisible_ = true;
	goFadingIn_ = true;
	goFadeCompleted_ = false;

	// UIを表示状態にする（ボタンはフェード完了後に表示）
	if (background_) background_->SetVisible(true);
	if (gameOver_) gameOver_->SetVisible(true);
	startButton_->SetVisible(false); // スタートボタンは非表示に

	goFadeDuration_ = (duration > 0.0f) ? duration : 0.001f;
	goFadeTimer_ = 0.0f;
	goAlpha_ = 0.0f;

	// 選択状態をリトライに初期化
	currentSelection_ = GameOverSelection::RETRY;

	// すぐ反映（0から開始）
	ApplyAlpha(goAlpha_);
}

/// <summary>
/// ゲームオーバーUIの状態をリセットし、描画を停止する
/// </summary>
void GameUI::ResetGameOver()
{
	goVisible_ = false;
	goFadingIn_ = false;
	goFadeCompleted_ = false;
	goFadeTimer_ = 0.0f;
	goFadeDuration_ = 0.6f;
	goAlpha_ = 0.0f;

	// UIを非表示にする
	if (background_) background_->SetVisible(false);
	if (gameOver_) gameOver_->SetVisible(false);
	if (retryButton_) {
		retryButton_->SetVisible(false);
		retryButton_->SetScale(retryButtonOriginalSize_);
	}
	if (titleButton_) {
		titleButton_->SetVisible(false);
		titleButton_->SetScale(titleButtonOriginalSize_);
	}
	startButton_->SetVisible(true);

	if (gameOver_) {
		ApplyAlpha(0.0f);
	}

	// 選択状態をリセット
	currentSelection_ = GameOverSelection::RETRY;

	// リクエストフラグもリセット
	ClearRequests();
}

/// <summary>
/// ポーズメニューの隠れ状態へ戻す（保存しても壊れない状態）。
/// スケール/色は作者値（pauseOriginalScales_/Colors_）に復元し、visible=false にする。
/// 開くときは OPENING アニメが t=0 から動くので、ここで作者値に戻しておいて問題ない。
/// </summary>
void GameUI::RestorePauseRestingState()
{
	for (auto& pair : pauseOriginalScales_) {
		UIBase* ui = pair.first;
		if (!ui) continue;
		ui->SetScale(pair.second);
		auto itCol = pauseOriginalColors_.find(ui);
		if (itCol != pauseOriginalColors_.end()) {
			ui->SetColor(itCol->second);
		}
		ui->SetVisible(false);
	}
}

/// <summary>
/// ポーズ画面用 UI を取得（無ければ layer2 で生成し configPath を持たせる）
/// </summary>
UIBase* GameUI::GetOrCreatePauseUI(const std::string& id, const std::string& texturePath,
	const Vector2& pos, const Vector2& size)
{
	auto* mgr = YoRigine::UIManager::GetInstance();
	if (UIBase* exist = mgr->GetUI(id)) return exist;  // 既にシーンにあればそれを使う

	const std::string cfg = "Resources/UIConfigs/GameScene/" + id + ".json";
	const bool hadCfg = std::filesystem::exists(cfg);

	auto ui = std::make_unique<UIBase>(id);
	ui->Initialize(cfg);   // 既存あれば位置/サイズ/テクスチャ復元、無ければ configPath_ を設定
	if (!hadCfg) {
		ui->SetTexture(texturePath);
		if (size.x > 0.0f && size.y > 0.0f) ui->SetSize(size); // 0 はテクスチャ実寸を使う
		ui->SetAnchorPoint({ 0.5f, 0.5f });
		ui->SetLayer(2);
		ui->SetPosition({ pos.x, pos.y, 0.0f });
		ui->SaveToJSON();  // 正しい初期値を保存（次回起動で復元）
	}
	UIBase* raw = ui.get();
	mgr->AddUI(id, std::move(ui));
	return raw;
}

// ラベル画像を（無ければ）ベイクするユーティリティ
static void BakeLabelIfMissing(const std::string& text, const std::string& path,
	float size, const Vector4& fill, float charSpacing = 0.0f,
	const std::string& font = "Resources/Fonts/kurobara-cinderella.ttf")
{
	if (std::filesystem::exists(path)) return;
	YoRigine::TextBakeParams tp;
	tp.text = text;
	tp.fontFilePath = font;
	tp.fontSize = size;
	tp.fillColor = fill;
	tp.outlineWidth = 3.0f; tp.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };  // 細め
	tp.padding = 6.0f;
	tp.charSpacing = charSpacing;  // 負で字間を詰める
	YoRigine::TextTextureBaker::Bake(tp, path);
}

// レイアウト用：UIを取得/生成し、位置・サイズ・色・レイヤーを強制設定する
UIBase* GameUI::SetupPauseUI(const std::string& id, const std::string& tex,
	const Vector2& pos, const Vector2& size, const Vector4& color)
{
	auto* mgr = YoRigine::UIManager::GetInstance();
	UIBase* ui = mgr->GetUI(id);
	if (!ui) {
		const std::string cfg = "Resources/UIConfigs/GameScene/" + id + ".json";
		auto up = std::make_unique<UIBase>(id);
		up->Initialize(cfg);
		ui = up.get();
		mgr->AddUI(id, std::move(up));
	}
	if (!tex.empty())               ui->SetTexture(tex);          // 空＝据え置き
	if (size.x > 0.0f && size.y > 0.0f) ui->SetSize(size);        // 0＝実寸据え置き
	ui->SetAnchorPoint({ 0.5f, 0.5f });
	ui->SetLayer(2);
	ui->SetPosition({ pos.x, pos.y, 0.0f });
	ui->SetColor(color);
	return ui;
}

// ポーズメニューを2パネルレイアウトで構築（左：PAUSE＋メニュー / 右：操作一覧）
void GameUI::BuildPauseLayout()
{
	using namespace PauseLayout;

	// ---- 画像パス ----
	const std::string white_tex   = "Resources/Textures/white.png";
	const std::string leftFrame   = "Resources/UITex/pause_frame_left.png";
	const std::string rightFrame  = "Resources/UITex/pause_frame_right.png";
	const std::string texPause    = "Resources/UITex/label_pause.png";
	const std::string texControls = "Resources/UITex/label_controls.png";
	const std::string texWeak     = "Resources/UITex/label_weak.png";
	const std::string texStrong   = "Resources/UITex/label_strong.png";
	const std::string texGuard    = "Resources/UITex/label_guard.png";
	const std::string texLockon   = "Resources/UITex/label_lockon.png";
	const std::string texCamReset = "Resources/UITex/label_camreset.png";
	const std::string texResume   = "Resources/UITex/label_resume.png";
	const std::string texTitle    = "Resources/UITex/label_title.png";
	const std::string texStick    = "Resources/Textures/Operation/Right stick.png";

	// ---- 必要な画像を用意（無ければベイク/生成） ----
	if (!std::filesystem::exists(leftFrame))
		YoRigine::TextTextureBaker::BakeRoundedRectFrame((int)kLeftW, (int)kLeftH, kFrameRadius, kFrameThickness, kGold, leftFrame);
	if (!std::filesystem::exists(rightFrame))
		YoRigine::TextTextureBaker::BakeRoundedRectFrame((int)kRightW, (int)kRightH, kFrameRadius, kFrameThickness, kGold, rightFrame);
	BakeLabelIfMissing("\xE3\x83\x9D\xE3\x83\xBC\xE3\x82\xBA", texPause, kPauseTitleSize, kGold, kTitleSpacing);       // ポーズ
	BakeLabelIfMissing("\xE6\x93\x8d\xE4\xBD\x9C\xE4\xB8\x80\xE8\xA6\xA7", texControls, kControlsTitleSize, kGold, kRowSpacing); // 操作一覧
	BakeLabelIfMissing("\xE5\xBC\xB1\xE6\x94\xBB\xE6\x92\x83", texWeak,   kRowFontSize, kOlive, kRowSpacing);          // 弱攻撃
	BakeLabelIfMissing("\xE5\xBC\xB7\xE6\x94\xBB\xE6\x92\x83", texStrong, kRowFontSize, kOlive, kRowSpacing);          // 強攻撃
	BakeLabelIfMissing("\xE3\x82\xAC\xE3\x83\xBC\xE3\x83\x89", texGuard,  kRowFontSize, kOlive, kRowSpacing);          // ガード
	BakeLabelIfMissing("\xE3\x83\xAD\xE3\x83\x83\xE3\x82\xAF\xE3\x82\xAA\xE3\x83\xB3", texLockon, kRowFontSize, kOlive, kRowSpacing); // ロックオン
	BakeLabelIfMissing("\xE3\x82\xAB\xE3\x83\xA1\xE3\x83\xA9\xE3\x83\xAA\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88", texCamReset, kRowFontSize, kOlive, kRowSpacing); // カメラリセット
	BakeLabelIfMissing("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\xB8\xE6\x88\xBB\xE3\x82\x8B", texResume, kMenuFontSize, kGold, kRowSpacing); // ゲームへ戻る
	BakeLabelIfMissing("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB", texTitle, kMenuFontSize, kGold, kRowSpacing); // タイトル

	const Vector2 leftPos { kLeftX,  kPanelY };
	const Vector2 rightPos{ kRightX, kPanelY };

	// ---- 背景・パネル ----
	SetupPauseUI("UIBackGround", "", { kScreenW * 0.5f, kScreenH * 0.5f }, { kScreenW, kScreenH }, kDim);
	SetupPauseUI("PauseLeftPanel",  white_tex, leftPos,  { kLeftW,  kLeftH },  kPanel);
	SetupPauseUI("PauseRightPanel", white_tex, rightPos, { kRightW, kRightH }, kPanel);
	SetupPauseUI("PauseLeftFrame",  leftFrame,  leftPos,  { kLeftW,  kLeftH },  kWhite);
	SetupPauseUI("PauseRightFrame", rightFrame, rightPos, { kRightW, kRightH }, kWhite);

	// ---- 左：タイトル＋ハイライト＋メニュー ----
	SetupPauseUI("PauseTitle", texPause, { kLeftX, kPauseTitleY }, { 0,0 }, kWhite);
	pauseSelectHighlight_ = SetupPauseUI("PauseSelectHighlight", white_tex, { kLeftX, kResumeY }, { kHighlightW, kHighlightH }, kHighlight);
	SetupPauseUI("toGame", texResume, { kLeftX, kResumeY },    { 0,0 }, kWhite); // ゲームへ戻る
	SetupPauseUI("title",  texTitle,  { kLeftX, kTitleItemY }, { 0,0 }, kWhite); // タイトル

	// ---- 右：操作一覧（アイコン＋ラベルの行をループ生成） ----
	SetupPauseUI("PauseControlsTitle", texControls, { kRightX, kControlsTitleY }, { 0,0 }, kWhite);

	struct CtrlRow { const char* iconId; std::string iconTex; const char* labelId; std::string labelTex; };
	const CtrlRow rows[] = {
		{ "A",          "",        "Weak",             texWeak   },  // A → 弱攻撃
		{ "B",          "",        "Strong",           texStrong },  // B → 強攻撃
		{ "LB",         "",        "Guard",            texGuard  },  // X(Xbutton) → ガード
		{ "LockOnIcon", texStick,  "PauseLockOnLabel", texLockon },  // R3 → ロックオン
	};
	for (int i = 0; i < (int)std::size(rows); ++i) {
		const float y = kRowStartY + kRowStepY * i;
		SetupPauseUI(rows[i].iconId,  rows[i].iconTex,  { kIconX,  y }, { kIconSize, kIconSize }, kWhite);
		SetupPauseUI(rows[i].labelId, rows[i].labelTex, { kLabelX, y }, { 0, 0 }, kWhite);
	}
	// 最終行：LB/RB（2アイコン）＋ カメラリセット
	const float camY = kRowStartY + kRowStepY * (float)std::size(rows);
	SetupPauseUI("PauseLB", "Resources/Textures/Operation/Ex_LB.png", { kIconX - kShoulderOffset, camY }, { kShoulderIconSize, kShoulderIconSize }, kWhite);
	SetupPauseUI("PauseRB", "Resources/Textures/Operation/Ex_RB.png", { kIconX + kShoulderOffset, camY }, { kShoulderIconSize, kShoulderIconSize }, kWhite);
	SetupPauseUI("PauseCamResetLabel", texCamReset, { kLabelX + kCamLabelOffset, camY }, { 0,0 }, kWhite); // カメラリセット

	// ---- 重なり順（背面→前面の順に最後尾へ送る） ----
	const char* zorder[] = {
		"UIBackGround",
		"PauseLeftPanel", "PauseRightPanel",
		"PauseLeftFrame", "PauseRightFrame",
		"PauseSelectHighlight",
		"PauseTitle", "PauseControlsTitle",
		"toGame", "title",
		"A", "B", "LB", "LockOnIcon", "PauseLB", "PauseRB",
		"Weak", "Strong", "Guard", "PauseLockOnLabel", "PauseCamResetLabel",
	};
	auto* mgr = YoRigine::UIManager::GetInstance();
	for (const char* id : zorder) mgr->BringToFront(id);
}

/// <summary>
/// 選択状態も考慮してアニメーションさせる
/// </summary>
/// <param name="t"></param>
void GameUI::UpdatePauseVisuals(float t)
{
	// アルファ値のフェード率 (0.0 ～ 1.0)
	float fadeFactor = std::clamp(t, 0.0f, 1.0f);

	for (auto& pair : pauseOriginalScales_) {
		UIBase* ui = pair.first;
		Vector2 originalSize = pair.second;

		if (!ui) continue;

		// スケール計算
		float targetScaleMultiplier = normalScale_;
		if ((ui == toGameButton_ && currentPauseSelection_ == PauseSelection::TO_GAME) ||
			(ui == toTitleButton_ && currentPauseSelection_ == PauseSelection::TO_TITLE)) {
			targetScaleMultiplier = selectionScale_;
		}
		ui->SetScale(originalSize * targetScaleMultiplier * t);

		// 保存しておいた元の色を取り出す
		Vector4 originalCol = pauseOriginalColors_[ui];
		Vector4 col = ui->GetColor();

		// RGBは元の色に戻す
		col.x = originalCol.x;
		col.y = originalCol.y;
		col.z = originalCol.z;
		col.w = originalCol.w * fadeFactor;

		ui->SetColor(col);
	}
}

/// <summary>
/// UIBaseへのアルファ値を適用
/// </summary>
/// <param name="a">アルファ値（0〜1）</param>
void GameUI::ApplyAlpha(float a)
{
	if (!gameOver_) return;

	a = std::clamp(a, 0.0f, 1.0f);

	// 色を適用
	Vector4 col = gameOver_->GetColor();
	Vector4 colBG = { 0.0f, 0.0f, 0.0f, 0.0f };

	colBG.w = a * 0.95f;
	col.w = a;

	gameOver_->SetColor(col);
	background_->SetColor(colBG);
}

/// <summary>
/// リクエストフラグをリセット
/// </summary>
void GameUI::ClearRequests()
{
	retryRequested_ = false;
	returnToTitleRequested_ = false;
}

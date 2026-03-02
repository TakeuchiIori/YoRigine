#include "GameUI.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/Input/Input.h>
#include <Systems/GameTime/GameTime.h>

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

	// レイヤー2（ポーズUI）を最初は非表示・透明にしておく
	auto layer2 = YoRigine::UIManager::GetInstance()->GetUIsByLayer(2);
	for (auto& ui : layer2) {
		pauseOriginalScales_[ui] = ui->GetScale();
		pauseOriginalColors_[ui] = ui->GetColor();
		ui->SetVisible(false);
		// 色情報の取得と透明化（初期化時にやっておく）
		Vector4 col = ui->GetColor();
		col.w = 0.0f;
		ui->SetColor(col);
		ui->SetScale(Vector2(0.0f, 0.0f)); // 最初はサイズ0
	}

	controlUI_ = std::make_unique<ControlUI>();
	controlUI_->Initialize();
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

				// 完全に消えたら非表示にする
				for (auto& pair : pauseOriginalScales_) {
					if (pair.first) pair.first->SetVisible(false);
				}
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
	}
	// 操作用UIの更新
	controlUI_->Update();



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
	controlUI_->Draw();
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
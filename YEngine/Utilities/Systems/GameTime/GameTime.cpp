#include "GameTime.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {
	/*------------------------------------------------------------------

							静的メンバ変数の定義

	-------------------------------------------------------------------*/
	GameTime::Clock::time_point GameTime::prevTime_;
	float GameTime::deltaTime_ = 0.0f;
	float GameTime::uiDeltaTime_ = 0.0f;
	float GameTime::vfxDeltaTime_ = 0.0f;
	float GameTime::unscaledDeltaTime_ = 0.0f;
	float GameTime::totalTime_ = 0.0f;
	float GameTime::fixedDeltaTime_ = std::chrono::duration<float, GameTime::FrameRate>(1).count();

	float GameTime::accumulatedTime_ = 0.0f;
	float GameTime::timeScale_ = 1.0f;
	float GameTime::vfxTimeScale_ = 1.0f;
		float GameTime::uiTimeScale_ = 1.0f;
		bool  GameTime::channelPaused_[static_cast<int>(TimeChannel::Count)] = { false, false, false };
	bool GameTime::isPause_ = false;
	bool GameTime::debugFreeze_ = false;
	bool GameTime::stepOneFrame_ = false;


	namespace {
		constexpr size_t kAvgFpsHistSize = 60; // 2秒間隔×120 ≒ 4分表示
		float   g_AvgFpsHist[kAvgFpsHistSize] = {};
		size_t  g_AvgFpsWrite = 0;
		bool    g_AvgFpsFilled = false;

		float averageFps_ = 0.0f;
		float fpsCounter_ = 0.0f;
		int frameCount_ = 0;

		constexpr float fpsInterval_ = 1.0f;

		float hitStopTimer_ = 0.0f;
		float hitStopDuration_ = 0.0f;
		float hitStopFreezeScale_ = 0.0f;   // 停止中の timeScale（0=完全停止 / 0.05=ほぼ停止）
		float hitStopEaseTimer_ = 0.0f;     // 停止明けの復帰ランプ残り時間
		float hitStopEaseDuration_ = 0.0f;  // 復帰ランプの総時間（0=即スナップ）

		float slowMotionTimer_ = 0.0f;
		float slowMotionSpeed_ = 1.0f;
	}



	void GameTime::Initialize()
	{
		prevTime_ = Clock::now();
		deltaTime_ = 0.0f;
		uiDeltaTime_ = 0.0f;
		vfxDeltaTime_ = 0.0f;
		unscaledDeltaTime_ = 0.0f;
		totalTime_ = 0.0f;
		accumulatedTime_ = 0.0f;
		timeScale_ = 1.0f;
		vfxTimeScale_ = 1.0f;
		fixedDeltaTime_ = std::chrono::duration<float, FrameRate>(1).count();
		isPause_ = false;
		debugFreeze_ = false;
		stepOneFrame_ = false;
	}

	void GameTime::Update()
	{
		auto now = Clock::now();
		std::chrono::duration<float> elapsed = now - prevTime_;
		prevTime_ = now;

		// 実時間の記録
		unscaledDeltaTime_ = elapsed.count();

		// デルタタイムの上限クランプ。
		// シーンロード / ブレークポイント / Alt-Tab 復帰などで prevTime_ からの経過が
		// 極端に大きくなると、その1フレームで patrolSpeed*dt 等が巨大化し、キャラが
		// 一気にワープしてしまう（例: フィールド敵がスポーン地点の外へ吹き飛ぶ）。
		// 10fps 相当を下限として、それ以上のスパイクは 1 フレーム分に丸める。
		constexpr float kMaxDeltaTime = 0.1f;
		unscaledDeltaTime_ = std::min(unscaledDeltaTime_, kMaxDeltaTime);

		// 停止判定
		//   editorFrozen : エディタ用フリーズ。全チャンネルを止める（1フレーム検証用）。
		//   gamePaused   : ゲーム本編ポーズ。Gameplay/Vfx は止めるが UI は動き続ける（メニューが反応する）。
		const bool editorFrozen = debugFreeze_ && !stepOneFrame_;
		const bool gamePaused   = isPause_   && !stepOneFrame_;

		const bool gPaused = channelPaused_[static_cast<int>(TimeChannel::Gameplay)];
		const bool uPaused = channelPaused_[static_cast<int>(TimeChannel::UI)];
		const bool vPaused = channelPaused_[static_cast<int>(TimeChannel::Vfx)];

		// --- UI チャンネル: 実時間×UIスケール。ゲームポーズ/ヒットストップの影響を受けない。
		uiDeltaTime_ = (editorFrozen || uPaused) ? 0.0f : (unscaledDeltaTime_ * uiTimeScale_);

		// --- Vfx チャンネル: 独自スケール。ゲームポーズ/エディタフリーズ/チャンネル停止で止まる。
		vfxDeltaTime_ = (editorFrozen || gamePaused || vPaused) ? 0.0f : (unscaledDeltaTime_ * vfxTimeScale_);

		// --- Gameplay チャンネル: ポーズ/フリーズ/チャンネル停止で停止。それ以外は timeScale_（hitstop/slowmo が駆動）。
		if (editorFrozen || gamePaused || gPaused) {
			deltaTime_ = 0.0f;
		} else {
			deltaTime_ = unscaledDeltaTime_ * timeScale_;
			totalTime_ += deltaTime_;
			accumulatedTime_ += deltaTime_;
		}
		stepOneFrame_ = false;



		// FPS計算（5秒ごとの平均）
		fpsCounter_ += unscaledDeltaTime_;
		frameCount_++;

		if (fpsCounter_ >= fpsInterval_) {
			averageFps_ = static_cast<float>(frameCount_) / fpsCounter_;
			frameCount_ = 0;
			fpsCounter_ = 0.0f;
		}

		// fpsCounter_リセット直後 (= 平均更新直後) に入れる
		if (fpsCounter_ == 0.0f) {
			g_AvgFpsHist[g_AvgFpsWrite] = averageFps_;
			g_AvgFpsWrite = (g_AvgFpsWrite + 1) % kAvgFpsHistSize;
			if (g_AvgFpsWrite == 0) { g_AvgFpsFilled = true; }
		}


		// ヒットストップ > スローモーション > 通常（Gameplay の timeScale_ を駆動）。
		// ポーズ/エディタフリーズ中はタイマーを進めない（再開時に続きから）。
		if (!editorFrozen && !gamePaused) {
			// 復帰先スケール（スロー中はスロー速度、通常は1.0）
			const float resumeScale = (slowMotionTimer_ > 0.0f) ? slowMotionSpeed_ : 1.0f;

			if (hitStopTimer_ > 0.0f) {
				// 停止フェーズ：timeScale を freezeScale に固定
				hitStopTimer_ -= unscaledDeltaTime_;
				timeScale_ = hitStopFreezeScale_;
				if (hitStopTimer_ <= 0.0f) {
					hitStopTimer_ = 0.0f;
					hitStopEaseTimer_ = hitStopEaseDuration_;      // 復帰ランプ開始
					if (hitStopEaseTimer_ <= 0.0f) timeScale_ = resumeScale; // ease無し=即スナップ
				}
			} else if (hitStopEaseTimer_ > 0.0f) {
				// 復帰フェーズ：freezeScale → resumeScale へ smoothstep で戻す
				hitStopEaseTimer_ -= unscaledDeltaTime_;
				float p = 1.0f - std::clamp(hitStopEaseTimer_ / (std::max)(hitStopEaseDuration_, 1e-4f), 0.0f, 1.0f);
				float s = p * p * (3.0f - 2.0f * p); // smoothstep
				timeScale_ = hitStopFreezeScale_ + (resumeScale - hitStopFreezeScale_) * s;
				if (hitStopEaseTimer_ <= 0.0f) {
					hitStopEaseTimer_ = 0.0f;
					timeScale_ = resumeScale;
				}
			} else if (slowMotionTimer_ > 0.0f) {
				slowMotionTimer_ -= unscaledDeltaTime_;
				timeScale_ = slowMotionSpeed_;
				if (slowMotionTimer_ <= 0.0f) {
					timeScale_ = 1.0f;
					slowMotionTimer_ = 0.0f;
				}
			}
		}


	}

	void GameTime::ImGui()
	{
#ifdef USE_IMGUI
		ImGui::Text("DeltaTime: %.6f", GameTime::GetDeltaTime());
		ImGui::Text("Total DeltaTime: %.2f", GameTime::GetAccumulatedTime());
		ImGui::Text("Unscaled DeltaTime: %f", GameTime::GetUnscaledDeltaTime());
		ImGui::Text("Total GamtTime: %.2f", GameTime::GetTotalTime());

		// エディタ停止は debugFreeze_（時間だけ止める。ゲームUIのポーズには波及しない）
		ImGui::Checkbox("Pause (エディタ停止)", &debugFreeze_);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("時間だけ止める。ゲーム本編のポーズUIには影響しない（操作ヒントが隠れない）");
		ImGui::Checkbox("Step One Frame", &stepOneFrame_);
		ImGui::SliderFloat("Time Scale (Gameplay)", &timeScale_, 0.0f, 2.0f);

		// ===== タイムチャンネル（チャンネル別 停止 / スケール）=====
		ImGui::SeparatorText("Time Channels");
		{
			bool gp = channelPaused_[static_cast<int>(TimeChannel::Gameplay)];
			bool up = channelPaused_[static_cast<int>(TimeChannel::UI)];
			bool vp = channelPaused_[static_cast<int>(TimeChannel::Vfx)];
			if (ImGui::Checkbox("Gameplay 停止##ch", &gp)) channelPaused_[static_cast<int>(TimeChannel::Gameplay)] = gp;
			ImGui::SameLine(); ImGui::Text("dt %.5f", GameTime::GetDeltaTime(TimeChannel::Gameplay));
			if (ImGui::Checkbox("UI 停止##ch", &up))       channelPaused_[static_cast<int>(TimeChannel::UI)] = up;
			ImGui::SameLine(); ImGui::Text("dt %.5f", GameTime::GetDeltaTime(TimeChannel::UI));
			if (ImGui::Checkbox("Vfx 停止##ch", &vp))      channelPaused_[static_cast<int>(TimeChannel::Vfx)] = vp;
			ImGui::SameLine(); ImGui::Text("dt %.5f", GameTime::GetDeltaTime(TimeChannel::Vfx));
			ImGui::SliderFloat("UI Time Scale",  &uiTimeScale_,  0.0f, 2.0f);
			ImGui::SliderFloat("Vfx Time Scale", &vfxTimeScale_, 0.0f, 2.0f);
		}

		// ===== ヒットストップ テスト =====
		ImGui::SeparatorText("HitStop テスト");
		static float testDur = 0.08f, testScale = 0.0f, testEase = 0.06f;
		ImGui::SliderFloat("停止時間(s)##hs",   &testDur,   0.0f, 0.5f, "%.3f");
		ImGui::SliderFloat("停止の強さ##hs",     &testScale, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("復帰イーズ(s)##hs", &testEase,  0.0f, 0.5f, "%.3f");
		if (ImGui::Button("テスト発火 (HitStop)")) {
			GameTime::SetHitStop(testDur, testScale, testEase);
		}

		// ===== FPSグラフ表示 =====
		ImGui::Text("FPS : %.2f", GameTime::GetAverageFPS());
		ImGui::SeparatorText("FPS Graph");

		// グラフ
		const int count = static_cast<int>(g_AvgFpsFilled ? kAvgFpsHistSize : g_AvgFpsWrite);
		const int offset = static_cast<int>(g_AvgFpsWrite);
		ImGui::PlotLines("Avg FPS (1s)", g_AvgFpsHist, count, offset,
			nullptr, 0.0f, 120.0f, ImVec2(0, 80));

#endif
	}

	void GameTime::Pause()
	{
		isPause_ = true;
	}

	void GameTime::Resume()
	{
		isPause_ = false;
	}

	bool GameTime::ShouldUpdateOneFrame()
	{
		if (accumulatedTime_ >= fixedDeltaTime_) {
			accumulatedTime_ -= fixedDeltaTime_;
			return true;
		}
		return false;
	}

	void GameTime::StepOneFrame()
	{
		stepOneFrame_ = true;

		// ポーズ中も進める
		isPause_ = true;
	}

	void GameTime::SetHitStop(float duration, float freezeScale, float easeOut)
	{
		// 多段ヒットのリフレッシュ：既に停止中なら「長い/硬い(小scale)/長イーズ」を採用。
		if (hitStopTimer_ > 0.0f) {
			hitStopTimer_        = (std::max)(hitStopTimer_, duration);
			hitStopFreezeScale_  = (std::min)(hitStopFreezeScale_, freezeScale);
			hitStopEaseDuration_ = (std::max)(hitStopEaseDuration_, easeOut);
		} else {
			hitStopTimer_        = duration;
			hitStopFreezeScale_  = freezeScale;
			hitStopEaseDuration_ = easeOut;
		}
		hitStopDuration_  = hitStopTimer_;
		hitStopEaseTimer_ = 0.0f;   // 新しい停止が入ったら復帰ランプは仕切り直し
	}

	void GameTime::SetSlowMotion(float duration, float speed) {
		slowMotionTimer_ = duration;
		slowMotionSpeed_ = speed;
	}

	bool GameTime::IsSlowMotion() {
		return slowMotionTimer_ > 0.0f;
	}

	float GameTime::GetAverageFPS()
	{
		return averageFps_;
	}
}

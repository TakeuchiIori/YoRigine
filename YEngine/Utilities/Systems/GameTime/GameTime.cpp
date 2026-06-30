#include "GameTime.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {
	/*------------------------------------------------------------------

							静的メンバ変数の定義

	-------------------------------------------------------------------*/
	GameTime::Clock::time_point GameTime::prevTime_;
	float GameTime::deltaTime_ = 0.0f;
	float GameTime::unscaledDeltaTime_ = 0.0f;
	float GameTime::totalTime_ = 0.0f;
	float GameTime::fixedDeltaTime_ = std::chrono::duration<float, GameTime::FrameRate>(1).count();

	float GameTime::accumulatedTime_ = 0.0f;
	float GameTime::timeScale_ = 1.0f;
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

		float slowMotionTimer_ = 0.0f;
		float slowMotionSpeed_ = 1.0f;
	}



	void GameTime::Initialize()
	{
		prevTime_ = Clock::now();
		deltaTime_ = 0.0f;
		unscaledDeltaTime_ = 0.0f;
		totalTime_ = 0.0f;
		accumulatedTime_ = 0.0f;
		timeScale_ = 1.0f;
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

		// ゲーム本編ポーズ or エディタ用フリーズのどちらかで時間を止める。
		// （ゲームUIは IsPause()=isPause_ だけを見るので、エディタフリーズには反応しない）
		if ((isPause_ || debugFreeze_) && !stepOneFrame_) {
			deltaTime_ = 0.0f;
			return;
		}

		// スケーリング
		deltaTime_ = unscaledDeltaTime_ * timeScale_;
		// 総合時間の記録
		totalTime_ += deltaTime_;
		// 固定時間の記録
		accumulatedTime_ += deltaTime_;
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


		// ヒットストップ > スローモーション > 通常
		if (hitStopTimer_ > 0.0f) {
			hitStopTimer_ -= unscaledDeltaTime_;
			timeScale_ = 0.0f;
			if (hitStopTimer_ <= 0.0f) {
				hitStopTimer_ = 0.0f;
				if (slowMotionTimer_ > 0.0f) {
					timeScale_ = slowMotionSpeed_;
				} else {
					timeScale_ = 1.0f;
				}
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
		ImGui::SliderFloat("Time Scale", &timeScale_, 0.0f, 2.0f);

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

	void GameTime::SetHitStop(float duration)
	{
		hitStopTimer_ = duration;
		hitStopDuration_ = duration;
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

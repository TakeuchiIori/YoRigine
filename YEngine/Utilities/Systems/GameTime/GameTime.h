#pragma once

// Engine
#include "float.h"
#include "MathFunc.h"

// C++
#include <chrono>
#include <ratio>

// 時間管理クラス（デルタタイム・ポーズ・スロー・ヒットストップなどを統括）
namespace YoRigine {
	class GameTime
	{
	public:
		///************************* 基本関数 *************************///

		// 初期化
		static void Initialize();

		// 更新
		static void Update();

		// ImGuiで表示
		static void ImGui();

	public:
		///************************* 時間制御 *************************///

		// 一時停止
		static void Pause();

		// 一時停止解除
		static void Resume();

		// 固定時間経過チェック
		static bool ShouldUpdateOneFrame();

		// デバッグ用：1フレームだけ進める
		static void StepOneFrame();

	public:
		///************************* 特殊時間処理 *************************///

		// ヒットストップを設定
		static void SetHitStop(float duration);

		// スローモーションを設定
		static void SetSlowMotion(float duration, float speed);

		// スローモーション中か確認
		static bool IsSlowMotion();

	public:
		///************************* アクセッサ *************************///

		static float GetDeltaTime() { return deltaTime_; }
		static float GetUnscaledDeltaTime() { return unscaledDeltaTime_; }
		static float GetAccumulatedTime() { return accumulatedTime_; }
		static float GetTotalTime() { return totalTime_; }
		static float GetFixedDeltaTime() { return fixedDeltaTime_; }
		static void SetTimeScale(float timeScale) { timeScale_ = timeScale; }
		static float GetTimeScale() { return timeScale_; }
		static bool IsPause() { return isPause_; }
		static float GetFPS() { return (deltaTime_ > 0.0f) ? 1.0f / deltaTime_ : 0.0f; }

		// 5秒ごとの平均FPSを取得
		static float GetAverageFPS();

	private:
		using Clock = std::chrono::steady_clock;
		
		// 60FPSという比率を定義
		using TargetFPS = std::ratio<1, 60>;

		// 1/60秒という同期をratioから逆算して定義
		using FrameRate = std::ratio<TargetFPS::den, TargetFPS::num>;

	private:
		///************************* 時間管理変数 *************************///

		static Clock::time_point prevTime_;
		static float deltaTime_;
		static float unscaledDeltaTime_;
		static float totalTime_;
		static float fixedDeltaTime_;
		static float accumulatedTime_;
		static float timeScale_;
		static bool isPause_;
		static bool stepOneFrame_;
	};

	///************************* FPS・時間関連の変数は GameTime.cpp 内の無名名前空間へ移動しました *************************///
}
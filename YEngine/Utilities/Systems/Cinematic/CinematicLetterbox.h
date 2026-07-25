#pragma once
#include <memory>

namespace YoRigine { class Sprite; }

namespace YoRigine {

	/// <summary>
	/// 映画風レターボックス（上下黒帯）
	/// SetProgress(0=完全隠れ / 1=完全表示) でスライドイン/アウト
	/// </summary>
	class CinematicLetterbox {
	public:
		CinematicLetterbox();
		~CinematicLetterbox();

		void Initialize();
		void Update();
		void Draw();

		// 0=完全隠れ（画面外）、1=完全表示
		void SetProgress(float progress);
		float GetProgress() const { return progress_; }

		// 画面高に対する黒帯の比率（上下それぞれ）。デフォルト 12%
		void SetBarHeightRatio(float ratio) { barHeightRatio_ = ratio; }

	private:
		std::unique_ptr<YoRigine::Sprite> topBar_;
		std::unique_ptr<YoRigine::Sprite> bottomBar_;
		float progress_ = 0.0f;
		float barHeightRatio_ = 0.12f;
	};

}

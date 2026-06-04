#pragma once
#include <memory>

namespace YoRigine {

	class CinematicLetterbox;
	class CinematicSequencer;

	/// <summary>
	/// 演出マネージャ（シングルトン）
	///   - アクティブな Sequencer を 1 本保持
	///   - 上下黒帯（Letterbox）の表示窓口
	///   - 各シーン共通の演出ハブ
	/// </summary>
	class CinematicManager {
	public:
		static CinematicManager* GetInstance();

		void Initialize();

		// 既存の演出があれば即停止して上書き
		void Play(std::unique_ptr<CinematicSequencer> seq);

		// 強制停止（Letterbox は隠す）
		void Stop();

		void Update(float deltaTime);
		void Draw();

		bool IsActive() const { return activeSeq_ != nullptr; }

		CinematicLetterbox* GetLetterbox() { return letterbox_.get(); }

	private:
		CinematicManager() = default;
		~CinematicManager();
		CinematicManager(const CinematicManager&) = delete;
		CinematicManager& operator=(const CinematicManager&) = delete;

		std::unique_ptr<CinematicLetterbox> letterbox_;
		std::unique_ptr<CinematicSequencer> activeSeq_;
	};

}

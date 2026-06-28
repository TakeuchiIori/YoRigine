#pragma once
#include <memory>
#include <map>
#include "TransitionFactory.h"

/// <summary>
/// シーン遷移を管理するクラス
/// </summary>
class TransitionManager {
public:
	static TransitionManager* GetInstance() {
		static TransitionManager instance;
		return &instance;
	};

	// 遷移タイプを設定
	void SetTransitionType(TransitionType type) {
		currentType_ = type;
	}

	// 現在の遷移タイプを取得
	TransitionType GetTransitionType() const {
		return currentType_;
	}

	// ファクトリーを作成
	std::unique_ptr<TransitionFactory> CreateFactory();

	// 遷移時間を設定
	void SetTransitionDuration(float duration) {
		transitionDuration_ = duration;
	}

	float GetTransitionDuration() const {
		return transitionDuration_;
	}

private:
	TransitionManager() = default;
	~TransitionManager() = default;
	TransitionManager(const TransitionManager&) = delete;
	TransitionManager& operator=(const TransitionManager&) = delete;

	TransitionType currentType_ = TransitionType::Fade;
	float transitionDuration_ = 1.0f;
};
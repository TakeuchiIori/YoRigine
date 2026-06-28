#include "TransitionManager.h"

/// <summary>
/// 現在設定されているトランジションタイプに応じて、対応するファクトリを生成する
/// </summary>
std::unique_ptr<TransitionFactory> TransitionManager::CreateFactory() {

	//------------------------------------------------------------
	// 現在のトランジションタイプに応じてファクトリ生成
	//------------------------------------------------------------
	switch (currentType_) {

	case TransitionType::Fade:
		return std::make_unique<FadeTransitionFactory>();
		break;

	default:
		return std::make_unique<FadeTransitionFactory>();
		break;
	}
}

#pragma once
#include <memory>
#include "ISceneTransition.h"
#include "../Fade/FadeTransition.h"

// 遷移の種類
enum class TransitionType {
	Fade,           // フェード
	Slide,          // スライド
	Wipe,           // ワイプ
	Mosaic,         // モザイク
	CircleWipe,     // 円形ワイプ
	CrossFade       // クロスフェード
};

// トランジションファクトリ基底クラス
class TransitionFactory {
public:
	virtual ~TransitionFactory() = default;
	virtual std::unique_ptr<ISceneTransition> CreateTransition() = 0;
	virtual TransitionType GetType() const = 0;
};

// フェード
class FadeTransitionFactory : public TransitionFactory {
public:
	std::unique_ptr<ISceneTransition> CreateTransition() override { return std::make_unique<FadeTransition>(); };
	TransitionType GetType() const override { return TransitionType::Fade; }
};


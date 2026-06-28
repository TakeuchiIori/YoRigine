#pragma once
// C++
#include <string>

// Engine
#include "BaseScene.h"


/// シーン工場（概念）
class AbstractSceneFactory {
public:
	/// 仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	/// シーン生成
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};
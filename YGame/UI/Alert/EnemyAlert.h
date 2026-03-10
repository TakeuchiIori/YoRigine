#pragma once
#include "Systems/UI/UIBase.h"
class EnemyAlert
{

public:

	void Initialize();

	void Update();

	void Draw();

private:

	UIBase* alertUI_ = nullptr;
	Vector2 defaultScale_{};
};


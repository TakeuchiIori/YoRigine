#pragma once
#include "CoreScenes./Factory/SceneFactory.h"
#include "Framework./Framework.h"
#include "OffScreen/OffScreen.h"

class MyGame : public Framework
{

public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

private: // メンバ変数
	std::unique_ptr<OffScreen> offScreen_;
};


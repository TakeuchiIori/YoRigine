#pragma once

// Engine
#include "CubeMap/CubeMap.h"
#include "WorldTransform/WorldTransform.h"
#include "Loaders/Json/JsonManager.h"

/// <summary>
/// スカイボックスクラス
/// </summary>
class SkyBox
{
public:
	///************************* 基本関数 *************************///

	void Initialize(Camera* camera, const std::string& textureFilePath);
	void Update();
	void Draw();

public:
	///************************* 公開関数 *************************///

	void SetTextureFilePath(const std::string& filePath);
private:
	///************************* 内部処理 *************************///

	void InitJson();
private:
	///************************* メンバ変数 *************************///

	std::unique_ptr<YoRigine::CubeMap> cubeMap_ = nullptr;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_ = nullptr;
};


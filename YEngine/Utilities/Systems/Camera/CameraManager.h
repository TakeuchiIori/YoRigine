#pragma once
#include "Camera.h"

// C++
#include <vector>
#include <memory>

// カメラの追加・削除・切り替え・更新を統括して管理するクラス
class CameraManager
{
public:
	///************************* 基本関数 *************************///

	// コンストラクタ
	CameraManager() = default;

public:
	///************************* カメラ生成・削除 *************************///

	// 新しいカメラを追加
	std::shared_ptr<Camera> AddCamera();

	// カメラを削除
	void RemoveCamera(std::shared_ptr<Camera> camera);

public:
	///************************* カメラ制御 *************************///

	// 現在使用中のカメラを設定
	void SetCurrentCamera(std::shared_ptr<Camera> camera);

	// 現在のカメラを取得
	std::shared_ptr<Camera> GetCurrentCamera() const;

	// すべてのカメラを更新
	void UpdateAllCameras();

private:
	///************************* メンバ変数 *************************///

	std::vector<std::shared_ptr<Camera>> cameras_;
	std::shared_ptr<Camera> currentCamera_;

};

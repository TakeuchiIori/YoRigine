#pragma once

// Engine
#include "MapChipField.h"
#include "MapChipCollision.h"
#include "Systems/Camera/Camera.h"
#include "Object3D/Object3d.h"
#include "WorldTransform/WorldTransform.h"

// C++
#include <vector>

///************************* マップチップ情報クラス *************************///
/// マップチップの生成・更新・描画を管理する
class MapChipInfo
{
public:
	///************************* デストラクタ *************************///
	/// 後始末を行う
	~MapChipInfo();

public:
	///************************* 基本関数 *************************///
	/// 初期化を行う
	void Initialize();

	/// 更新を行う
	void Update();

	/// 描画を行う
	void Draw();

public:
	///************************* カメラ設定 *************************///
	/// 使用するカメラを設定する
	void SetCamera(Camera* camera) { camera_ = camera; }

private:
	///************************* 内部処理 *************************///
	/// ブロックオブジェクトを生成する
	void GenerateBlocks();

public:
	///************************* マップデータ設定 *************************///
	/// マップフィールドを設定する
	void SetMapChipField(std::unique_ptr<MapChipField> mpField) { mpField_ = std::move(mpField); }

	/// マップフィールドを取得する
	MapChipField* GetMapChipField() { return mpField_.get(); }

private:
	///************************* メンバ変数 *************************///
	/// 使用中のカメラ
	Camera* camera_ = nullptr;

	/// 各ブロックのワールド変換
	std::vector<std::vector<std::unique_ptr<WorldTransform>>> wt_;

	/// マップチップデータ
	std::unique_ptr<MapChipField> mpField_ = nullptr;

	/// 各ブロックの描画オブジェクト
	std::vector<std::vector<std::unique_ptr<Object3d>>> objects_;
};

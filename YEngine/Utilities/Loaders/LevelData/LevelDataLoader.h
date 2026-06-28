#pragma once

// C++
#include "json.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>

// Engine
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"

///************************* レベルデータローダークラス *************************///
///
/// BlenderなどからエクスポートされたJSONデータを解析し、
/// ゲームシーン上にオブジェクトを自動生成・配置するクラス。
/// モデル、トランスフォーム、コライダー情報などを読み取り、
/// 階層構造を再現してシーンを構築する。
///
class LevelDataLoader
{
public:
	///************************* 内部構造体 *************************///

	// オブジェクト情報構造体
	struct ObjectData {
		std::string fileName;        // モデルファイル名
		std::string name;            // オブジェクト名（Blenderの "name"）
		Vector3 translation;         // 位置（translation）
		Vector3 rotation;            // 回転（rotation）
		Vector3 scale;               // スケーリング（scaling）

		// コライダー情報
		std::string colliderType;    // コライダーの種類（Sphere, AABB, OBBなど）
		Vector3 colliderCenter;      // コライダー中心位置
		Vector3 colliderSize;        // コライダーサイズ

		// 子オブジェクト（階層構造対応）
		std::vector<ObjectData> children;
	};

	// レベル全体の情報構造体
	struct LevelData {
		std::vector<ObjectData> objData; // 含まれるオブジェクト一覧
	};

public:
	///************************* 基本処理 *************************///

	// 初期化処理（ファイルロード準備）
	void Initialize();

	// ファイル存在チェック（JSONパス検証）
	void FileCheck();

	// JSONファイル内のオブジェクトを走査・解析
	void ScanningObjects();

	// JSONオブジェクトを再帰的に読み取り、階層構造を再現
	void LoadObjectRecursive(const nlohmann::json& jsonObject, ObjectData& parent);

	// 読み込んだデータをもとにシーンを構築
	void SetScene();

public:
	///************************* 更新・描画 *************************///

	// フレーム更新処理
	void Update();

	// 読み込んだ全オブジェクトを描画
	void Draw(Camera* camera);

private:
	///************************* メンバ変数 *************************///

	static const std::string defaultPath;			// デフォルトの読み込みパス
	static const std::string defaultFileName;		// デフォルトのファイル名
	static const std::string defaultModelPath_;		// モデル格納パス

	nlohmann::json deserialized_;					// デシリアライズ済みJSONデータ

	std::unique_ptr<LevelData> levelData_;			// レベルデータ本体
	std::map<std::string, Model*> models_;			// モデルキャッシュ

	std::vector<std::unique_ptr<Object3d>> objects_;		 // シーン上の3Dオブジェクト
	std::vector<std::unique_ptr<WorldTransform>> worldTransforms_; // オブジェクトの変換情報
};

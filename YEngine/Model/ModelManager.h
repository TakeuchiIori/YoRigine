#pragma once

// C++
#include <string>
#include <map>
#include <memory>
#include <string>
#include <string_view>

// Engine
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"

// モデルを一元管理するクラス
class ModelManager
{
public:
	///************************* シングルトン *************************///

	// インスタンス取得
	static ModelManager* GetInstance();

	// 終了処理
	void Finalize();

	// コンストラクタ
	ModelManager() = default;
	~ModelManager() = default;

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialze(YoRigine::DirectXCommon* dxCommon);

	// モデル読み込み
	void LoadModel(const std::string& directoryPath, const std::string& filePath, const std::string& animationName = "", bool isAnimation = false);

	// モデル検索
	Model* FindModel(const std::string& filePath, const std::string& animationName = "", bool isAnimation = false);

	// パスから「拡張子なしのベース名」と「拡張子付きファイル名」を分離する
	std::pair<std::string, std::string> ParseModelPath(const std::string& filePath);

public:
	///************************* アクセッサ *************************///

	// 登録されているすべてのモデルのキー一覧を返す
	std::vector<std::string> GetModelKeys() const;

	// Model* を直接返す一覧（必要なら）
	std::vector<Model*> GetAllModels() const;
private:
	///************************* シングルトン管理 *************************///

	// インスタンス
	static std::unique_ptr<ModelManager> instance;
	static std::once_flag initInstanceFlag;

	// コピー禁止
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;

private:
	///************************* メンバ変数 *************************///

	// モデルリスト
	std::map<std::string, std::unique_ptr<Model>> models;
};

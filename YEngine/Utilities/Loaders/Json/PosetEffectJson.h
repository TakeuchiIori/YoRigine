#pragma once

// C++
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <format>
#include <json.hpp>
#include <unordered_set>
#include <Windows.h>

// Engine
#include "ConversionJson.h"
#include "VariableJson.h"


// ポストエフェクトのパラメータをJSON形式で保存・読み込みする
// 各種エフェクト設定をImGuiと連動して管理する
class PosetEffectJson
{
public:
	///************************* コンストラクタ・デストラクタ *************************///

	// ファイル名とフォルダを指定して初期化
	PosetEffectJson(const std::string& fileName, const std::string& folderPath);

	// 登録データを破棄
	~PosetEffectJson();

public:
	///************************* 変数登録 *************************///

	// 指定した変数を登録してJSONから読み込む
	template <typename T>
	void Register(const std::string& name, T* ptr)
	{
		std::string fullKey;

		if (!treePrefix_.empty()) {
			fullKey = treePrefix_ + "." + name;
			treeKeys_.insert(fullKey);
		} else {
			fullKey = name;
		}

		variables_[fullKey] = std::make_unique<VariableJson<T>>(ptr);
		LoadAll();
	}

	// 指定した変数を登録解除
	void Unregister(const std::string& name);

	// 登録済み変数を初期状態にリセット
	void Reset(bool clearVariables = false);

public:
	///************************* 保存・読み込み *************************///

	// 登録された変数をすべてJSONファイルに保存
	void Save();

	// 登録された変数をすべてJSONファイルから読み込み
	void LoadAll();

public:
	///************************* ImGui操作 *************************///

	// 登録変数をImGui上に表示・編集
	static void ImGuiManager();

	// 登録を全解除
	void ClearRegister(std::string parentFileName);

public:
	///************************* カテゴリ管理 *************************///

	// カテゴリ設定
	void SetCategory(const std::string& category) { category_ = category; }

	// カテゴリ取得
	const std::string& GetCategory() const { return category_; }

	// サブカテゴリ設定
	void SetSubCategory(const std::string& subCategory) { subCategory_ = subCategory; }

	// サブカテゴリ取得
	const std::string& GetSubCategory() const { return subCategory_; }

	// ツリープレフィックス設定（階層表示用）
	void SetTreePrefix(const std::string& prefix) { treePrefix_ = prefix; }

	// ツリープレフィックス解除
	void ClearTreePrefix() { treePrefix_.clear(); }

	// 表示名取得
	const std::string& GetDisplayName() const { return displayName_; }

private:
	///************************* 内部処理 *************************///

	// フォルダとファイル名から完全パスを生成
	std::string MakeFullPath(const std::string& folder, const std::string& file) const;

private:
	///************************* メンバ変数 *************************///

	std::string fileName_;							// 保存ファイル名
	std::string folderPath_;						// 保存フォルダパス
	std::string displayName_;						// 表示名（ImGui表示用）

	std::unordered_map<std::string, std::unique_ptr<IVariableJson>> variables_; // 登録変数
	std::unordered_map<std::string, bool> child_;								// 子要素管理

	static inline std::unordered_map<std::string, PosetEffectJson*> instances;	// 全インスタンス管理
	static inline std::string selectedClass;									// 選択中のクラス名

	std::string category_;								// カテゴリ名
	std::string subCategory_;							// サブカテゴリ名
	std::string treePrefix_;							// ツリー構造用プレフィックス
	std::unordered_set<std::string> treeKeys_;			// 登録キー一覧
};

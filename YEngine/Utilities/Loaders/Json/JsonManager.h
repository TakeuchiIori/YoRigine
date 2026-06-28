#pragma once

// C++
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <format>
#include <json.hpp>
#include <Windows.h>

// Engine
#include "ConversionJson.h"
#include "VariableJson.h"

///************************* JSON管理クラス *************************///
/// 各シーンやカテゴリごとの変数をJSON形式で保存・読み込みする
/// 登録された変数の永続化と、ImGui上での編集をサポートする

namespace YoRigine {
	class JsonManager
	{
	public:
		///************************* コンストラクタ・デストラクタ *************************///

		// 指定ファイル名とフォルダを設定して初期化
		JsonManager(const std::string& fileName, const std::string& folderPath);

		// 登録データを解放
		~JsonManager();

	public:
		///************************* 変数登録 *************************///

		// 変数を登録し、自動的にJSONから読み込む
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

		// 変数を初期状態にリセット
		void Reset(bool clearVariables = false);

	public:
		///************************* 読み込み・保存 *************************///

		// 登録済みの変数をすべてJSONファイルに保存
		void Save();

		// 登録済みの変数をすべてJSONファイルから読み込み
		void LoadAll();

	public:
		///************************* ImGui表示 *************************///

		// ImGui上に登録済み変数をツリー表示
		static void ImGuiManager();

	public:
		///************************* シーン管理 *************************///

		// 指定ファイル名の登録をクリア
		void ClearRegister(std::string parentFileName);

		// 現在のシーン名を設定
		static void SetCurrentScene(const std::string& sceneName);

		// 指定シーンの全インスタンスをクリア
		static void ClearSceneInstances(const std::string& sceneName);

		// 現在のシーン名を取得
		static const std::string& GetCurrentScene() { return currentScene_; }

	public:
		///************************* カテゴリ設定 *************************///

		// カテゴリを設定
		void SetCategory(const std::string& category) { category_ = category; }

		// カテゴリを取得
		const std::string& GetCategory() const { return category_; }

		// サブカテゴリを設定
		void SetSubCategory(const std::string& subCategory) { subCategory_ = subCategory; }

		// サブカテゴリを取得
		const std::string& GetSubCategory() const { return subCategory_; }

		// 階層付きプレフィックスを設定（ツリー構造用）
		void SetTreePrefix(const std::string& prefix) { treePrefix_ = prefix; }

		// プレフィックスをクリア
		void ClearTreePrefix() { treePrefix_.clear(); }

		// 登録ファイルの表示名を取得
		const std::string& GetDisplayName() const { return displayName_; }

	private:
		///************************* 内部処理 *************************///

		// フォルダとファイル名を結合して完全パスを生成
		std::string MakeFullPath(const std::string& folder, const std::string& file) const;

	private:
		///************************* メンバ変数 *************************///

		// ファイル名
		std::string fileName_;

		// 保存フォルダパス
		std::string folderPath_;

		// 表示名（ImGuiなどで使用）
		std::string displayName_;

		// 完全キー名（カテゴリー＋名前）
		std::string fullKey_;

		// 所属シーン名
		std::string sceneName_;

		// 登録済み変数マップ
		std::unordered_map<std::string, std::unique_ptr<IVariableJson>> variables_;

		// 子要素管理フラグ
		std::unordered_map<std::string, bool> child_;

		// シーンごとのJsonManagerインスタンス管理
		static inline std::unordered_map<std::string, std::unordered_map<std::string, JsonManager*>> sceneInstances_;

		// 現在アクティブなシーン
		static inline std::string currentScene_;

		// 現在選択中のクラス名（エディタ用）
		static inline std::string selectedClass_;

		// カテゴリ名
		std::string category_;

		// サブカテゴリ名
		std::string subCategory_;

		// ツリー用プレフィックス
		std::string treePrefix_;

		// 登録されたキーの集合
		std::unordered_set<std::string> treeKeys_;

	private:
		///************************* シーンインスタンス取得 *************************///

		// 現在のシーンに紐づくJsonManagerインスタンス一覧を取得
		static std::unordered_map<std::string, JsonManager*>& GetCurrentSceneInstances();
	};
}
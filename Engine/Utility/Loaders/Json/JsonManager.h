#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <format>
#include <json.hpp>
#include "Windows.h"
#include "ConversionJson.h"
#include "VariableJson.h"

/// <summary>
///  JSON を使って登録した変数を一括管理するクラス
/// </summary>
class JsonManager
{
public:
	/// <summary>
	///  コンストラクタ
	/// </summary>
	/// <param name="fileName">保存するファイル名（例: "data.json"）</param>
	/// <param name="folderPath">保存先フォルダへのパス（末尾に'/'や'\\'は不要でも良い）</param>
	JsonManager(const std::string& fileName, const std::string& folderPath);


	/// <summary>
	///  デストラクタ
	/// </summary>
	~JsonManager();

	/// <summary>
	///  変数を登録する
	/// </summary>
	/// <typeparam name="T">登録する変数の型</typeparam>
	/// <param name="name">JSON 内でのキーとなる文字列</param>
	/// <param name="ptr">登録する変数のポインタ</param>
	template <typename T>
	void Register(const std::string& name, T* ptr)
	{
		variables_[name] = std::make_unique<VariableJson<T>>(ptr);
		LoadAll();
	}

	void Reset(bool clearVariables = false);

	/// <summary>
	///  登録した変数をすべて保存する
	/// </summary>
	void Save();

	/// <summary>
	///  登録した変数をすべて読み込む
	/// </summary>
	void LoadAll();

	/// <summary>
	/// ImGui
	/// </summary>
	/// <param name="className"></param>
	static void ImGui(const std::string& className);

	/// <summary>
	/// 実際にImGuiを表示する
	/// </summary>
	static void ImGuiManager();

private:
	/// <summary>
	///  フォルダパスとファイル名からフルパス文字列を組み立てる
	/// </summary>
	/// <param name="folder">フォルダパス</param>
	/// <param name="file">ファイル名</param>
	/// <returns>フルパス文字列</returns>
	std::string MakeFullPath(const std::string& folder, const std::string& file) const;

private:
	// 保存先のファイル名
	std::string fileName_;
	// 保存先のフォルダパス
	std::string folderPath_;
	// 登録名 -> 変数オブジェクト
	std::unordered_map<std::string, std::unique_ptr<IVariableJson>> variables_;
	static inline std::unordered_map<std::string, JsonManager*> instances;
	static inline std::string selectedClass;
};

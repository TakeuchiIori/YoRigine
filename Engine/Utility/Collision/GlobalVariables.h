#pragma once
// C++
#include <variant>
#include <map>
#include <string>
#include <json.hpp>

// Math
#include "Vector3.h"
#include "Quaternion.h"

using json = nlohmann::json;

/// <summary>
/// グローバル変数
/// </summary>
const std::string kDirectoryPath = "Resources/JSON/GlobalVariables/";

class GlobalVariables {
public: // インナークラス

	// 項目
	struct Item {
		// 項目の値
		std::variant<int32_t, float, Vector3, Quaternion, bool> value;
	};

	// グループ
	struct Group {
		std::map<std::string, Item> items;
	};

public: // メンバ関数

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static GlobalVariables* GetInstance();

	/// <summary>
	/// グループの作成
	/// </summary>
	void CreateGroup(const std::string& groupName);

	/// <summary>
	/// ファイルの書き出し
	/// </summary>
	void SaveFile(const std::string& groupName);

	/// <summary>
	/// ディレクトリの全ファイル読み込み
	/// </summary>
	void LoadFiles();

	/// <summary>
	/// ファイルから読み込む
	/// </summary>
	/// <param name="groupName"></param>
	void LoadFile(const std::string& groupName);

	/// <summary>
	/// 項目の追加(int)
	/// </summary>
	void AddItem(const std::string& groupName, const std::string& key, int32_t value);

	/// <summary>
	/// 項目の追加(float)
	/// </summary>
	void AddItem(const std::string& groupName, const std::string& key, float value);

	/// <summary>
	/// 項目の追加(Vector3)
	/// </summary>
	void AddItem(const std::string& groupName, const std::string& key, const Vector3& value);
	
	/// <summary>
	/// 項目の追加(Quaternion)
	/// </summary>
	void AddItem(const std::string& groupName, const std::string& key, const Quaternion& value);

	/// <summary>
	/// 項目の追加(bool)
	/// </summary>
	void AddItem(const std::string& groupName, const std::string& key, const bool& value);

	// 値のセット（int）
	void SetValue(const std::string& groupName, const std::string& key, int32_t value);
	// 値のセット（float）
	void SetValue(const std::string& groupName, const std::string& key, float value);
	// 値のセット（Vector3）
	void SetValue(const std::string& groupName, const std::string& key, const Vector3 value);
	// 値のセット（Vector3）
	void SetValue(const std::string& groupName, const std::string& key, const Quaternion value);
	// 値のセット (bool)
	void SetValue(const std::string& groupName, const std::string& key, const bool& value);



public: // アクセッサ

	int32_t GetIntValue(const std::string& groupName, const std::string& key) const;
	float GetFloatValue(const std::string& groupName, const std::string& key) const;
	Vector3 GetVector3Value(const std::string& groupName, const std::string& key) const;
	bool GetBoolValue(const std::string& groupName, const std::string& key) const;
	Quaternion GetQuaternionValue(const std::string& groupName, const std::string& key) const;

private:
	// コンストラクタ
	GlobalVariables() = default;
	// デストラクタ
	~GlobalVariables() = default;
	// コピーコンストラクタを無効
	GlobalVariables(const GlobalVariables& obj) = delete;
	// 代入演算子を無効
	GlobalVariables& operator=(const GlobalVariables& obj) = delete;

	// 全データ
	std::map<std::string, Group> datas_;



};



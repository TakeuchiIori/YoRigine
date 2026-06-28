#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <typeindex>

///************************* Enum情報をJsonManagerに登録するクラス *************************///
///  Enum の表示名を管理するレジストリ
///  REGISTER_ENUM から登録され、ImGui や JsonManager が参照する
class EnumRegistry
{
public:

	/// <summary>
	///  シングルトン取得
	/// </summary>
	static EnumRegistry& GetInstance() {
		static EnumRegistry instance;
		return instance;
	}

	/// <summary>
	///  Enum 型に表示名リストを登録
	/// </summary>
	void RegistryEnum(std::type_index type, const std::vector<std::string>& names) {
		registry_[type] = names;
	}

	/// <summary>
	///  指定 Enum 型の表示名を取得
	/// </summary>
	const std::vector<std::string>& GetNames(std::type_index type) const {
		static const std::vector<std::string> empty;
		auto it = registry_.find(type);
		return (it != registry_.end()) ? it->second : empty;
	}

private:
	///************************* シングルトンのためコピーとムーブを禁止 *************************///
	EnumRegistry() = default;
	~EnumRegistry() = default;

	EnumRegistry(const EnumRegistry&) = delete;
	EnumRegistry& operator=(const EnumRegistry&) = delete;
	EnumRegistry(EnumRegistry&&) = delete;
	EnumRegistry& operator=(EnumRegistry&&) = delete;

private:
	//  Enum 型 → 表示名リスト
	std::unordered_map<std::type_index, std::vector<std::string>> registry_;
};

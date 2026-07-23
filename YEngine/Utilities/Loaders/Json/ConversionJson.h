#pragma once

// C++
#include <json.hpp>
#include <vector>
#include <list>
#include <unordered_map>
#include <tuple>
#include <string>
#include <variant>
#include <cstddef>

// Math
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Quaternion.h"

using json = nlohmann::json;

///************************* JSON変換テンプレート群 *************************///
///
/// 各種データ型をnlohmann::json形式へ変換するための汎用ユーティリティ
/// 基本型・STLコンテナ・数学構造体などをサポート
///

///************************* 基本型 *************************///

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value || std::is_same<T, std::string>::value>::type
to_json(json& j, const T& value)
{
	j = value;
}

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value || std::is_same<T, std::string>::value>::type
from_json(const json& j, T& value)
{
	j.get_to(value);
}

///************************* STLコンテナ *************************///

template <typename T>
void to_json(json& j, const std::vector<T>& container)
{
	j = json::array();
	for (const auto& item : container)
	{
		j.push_back(item);
	}
}

template <typename T>
void from_json(const json& j, std::vector<T>& container)
{
	container.clear();
	for (const auto& item : j)
	{
		container.push_back(item.get<T>());
	}
}

template <typename T>
void to_json(json& j, const std::list<T>& container)
{
	j = json::array();
	for (const auto& item : container)
	{
		j.push_back(item);
	}
}

template <typename T>
void from_json(const json& j, std::list<T>& container)
{
	container.clear();
	for (const auto& item : j)
	{
		container.push_back(item.get<T>());
	}
}

///************************* 連想配列 *************************///

template <typename K, typename V>
void to_json(json& j, const std::unordered_map<K, V>& map)
{
	for (const auto& pair : map)
	{
		j[std::to_string(pair.first)] = pair.second;
	}
}

template <typename K, typename V>
void from_json(const json& j, std::unordered_map<K, V>& map)
{
	map.clear();
	for (auto it = j.begin(); it != j.end(); ++it)
	{
		map[static_cast<K>(std::stoi(it.key()))] = it.value().get<V>();
	}
}

///************************* ペア型 *************************///

template <typename T1, typename T2>
void to_json(json& j, const std::pair<T1, T2>& p)
{
	j = json{ p.first, p.second };
}

template <typename T1, typename T2>
void from_json(const json& j, std::pair<T1, T2>& p)
{
	p.first = j[0].get<T1>();
	p.second = j[1].get<T2>();
}

///************************* タプル型 *************************///

template <typename... Args>
void to_json(json& j, const std::tuple<Args...>& t)
{
	j = json::array();
	std::apply([&](const auto&... args) { ((j.push_back(args)), ...); }, t);
}

template <typename... Args>
void from_json(const json& j, std::tuple<Args...>& t)
{
	std::apply([&](auto&... args)
		{ ((args = j[&args - &std::get<0>(t)].get<std::decay_t<decltype(args)>>()), ...); }, t);
}

///************************* 数学構造体 *************************///

inline void to_json(json& j, const Vector2& v)
{
	j = json{ {"x", v.x}, {"y", v.y} };
}

inline void from_json(const json& j, Vector2& v)
{
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

inline void to_json(json& j, const Vector3& v)
{
	j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

inline void from_json(const json& j, Vector3& v)
{
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
	j.at("z").get_to(v.z);
}

inline void to_json(json& j, const Vector4& v)
{
	j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
}

inline void from_json(const json& j, Vector4& v)
{
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
	j.at("z").get_to(v.z);
	j.at("w").get_to(v.w);
}

inline void to_json(json& j, const Quaternion& q)
{
	j = json{ {"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w} };
}

inline void from_json(const json& j, Quaternion& q)
{
	j.at("x").get_to(q.x);
	j.at("y").get_to(q.y);
	j.at("z").get_to(q.z);
	j.at("w").get_to(q.w);
}

///************************* variant（型安全union） *************************///
///
/// nlohmann::adl_serializer を特殊化することで、名前空間に依存せず
/// 任意の std::variant を JSON 化できる（global の to_json は
/// std 型の ADL で見つからないため、こちらで対応する）。
/// 形式: { "_index": アクティブ番号, "_data": 中身 }
/// _data の各候補型は自分の to_json/from_json を持つこと。
///
namespace nlohmann {

	// index に一致する候補型で _data を復元する（再帰でインデックス探索）
	template <typename Variant, std::size_t I = 0>
	inline void VariantFromJsonAt(const json& j, std::size_t index, Variant& out)
	{
		if constexpr (I < std::variant_size_v<Variant>)
		{
			if (I == index)
			{
				using Alt = std::variant_alternative_t<I, Variant>;
				out.template emplace<I>(j.at("_data").template get<Alt>());
				return;
			}
			VariantFromJsonAt<Variant, I + 1>(j, index, out);
		}
	}

	template <typename... Ts>
	struct adl_serializer<std::variant<Ts...>>
	{
		static void to_json(json& j, const std::variant<Ts...>& v)
		{
			j = json::object();
			j["_index"] = static_cast<int>(v.index());
			std::visit([&j](const auto& value) { j["_data"] = value; }, v);
		}

		static void from_json(const json& j, std::variant<Ts...>& v)
		{
			const std::size_t index = static_cast<std::size_t>(j.value("_index", 0));
			VariantFromJsonAt<std::variant<Ts...>, 0>(j, index, v);
		}
	};

} // namespace nlohmann

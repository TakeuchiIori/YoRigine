#pragma once

// C++
#include <json.hpp>
#include <memory>
#include <string>

// Engine
#include "ConversionJson.h"
#include "EnumRegistry.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG

///************************* JSON変数インターフェース *************************///
///
/// 登録された変数をJSON変換・読み込み・ImGui編集できるようにする抽象クラス
///
class IVariableJson
{
public:
	///************************* デストラクタ *************************///

	virtual ~IVariableJson() = default;

public:
	///************************* 純粋仮想関数 *************************///

	// JSONへ保存
	virtual void SaveToJson(nlohmann::json& j) const = 0;

	// JSONから読み込み
	virtual void LoadFromJson(const nlohmann::json& j) = 0;

	// ImGui上に変数を表示
	virtual void ShowImGui(const std::string& name, const std::string& uniqueID) = 0;

	// 登録変数を初期値にリセット
	virtual void ResetValue() = 0;
};

///************************* 汎用テンプレートクラス *************************///
///
/// 任意の型Tに対応したJSON保存・読み込み・ImGui編集を行う
///
template <typename T>
class VariableJson : public IVariableJson
{
public:
	///************************* コンストラクタ *************************///

	// 登録対象ポインタを受け取って保持
	VariableJson(T* ptr)
		: ptr_(ptr)
	{
	}

public:
	///************************* JSON保存 *************************///

	// 登録変数をJSONオブジェクトに書き出す
	void SaveToJson(nlohmann::json& j) const override
	{
		j = *ptr_;
	}

public:
	///************************* JSON読み込み *************************///

	// JSONオブジェクトから変数を読み込む
	void LoadFromJson(const nlohmann::json& j) override
	{
		if (!j.is_null())
		{
			*ptr_ = j.get<T>();
		}
	}

public:
	///************************* ImGui表示 *************************///

	// 型ごとに適切なImGuiウィジェットを表示
	void ShowImGui([[maybe_unused]] const std::string& name, [[maybe_unused]] const std::string& uniqueID) override
	{
#ifdef USE_IMGUI
		std::string label = name + "##" + uniqueID;

		// 整数型（bool以外）
		if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
		{
			int tmp = static_cast<int>(*ptr_);
			if (ImGui::DragInt(label.c_str(), &tmp))
			{
				*ptr_ = static_cast<T>(tmp);
			}
		}
		// 浮動小数点型
		else if constexpr (std::is_floating_point_v<T>)
		{
			float tmp = static_cast<float>(*ptr_);
			if (ImGui::DragFloat(label.c_str(), &tmp, 0.1f))
			{
				*ptr_ = static_cast<T>(tmp);
			}
		}
		// 真偽値
		else if constexpr (std::is_same_v<T, bool>)
		{
			ImGui::Checkbox(label.c_str(), ptr_);
		}
		// 文字列
		else if constexpr (std::is_same_v<T, std::string>)
		{
			char buffer[256];
			strncpy_s(buffer, ptr_->c_str(), sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\0';
			if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
			{
				*ptr_ = std::string(buffer);
			}
		}
		// Vector2
		else if constexpr (std::is_same_v<T, Vector2>)
		{
			ImGui::DragFloat2(label.c_str(), reinterpret_cast<float*>(ptr_), 0.1f);
		}
		// Vector3
		else if constexpr (std::is_same_v<T, Vector3>)
		{
			ImGui::DragFloat3(label.c_str(), reinterpret_cast<float*>(ptr_), 0.1f);
		}
		// Vector4
		else if constexpr (std::is_same_v<T, Vector4>)
		{
			ImGui::DragFloat4(label.c_str(), reinterpret_cast<float*>(ptr_), 0.1f);
		}
		// Quaternion
		else if constexpr (std::is_same_v<T, Quaternion>)
		{
			ImGui::DragFloat4(label.c_str(), reinterpret_cast<float*>(ptr_), 0.1f);
		}
		// Enum（列挙型）
		else if constexpr (std::is_enum_v<T>)
		{
			const auto& names = EnumRegistry::GetInstance().GetNames(typeid(T));

			// 未登録の Enum に対するフォールバック
			if (names.empty()) {
				ImGui::Text("%s : (Enum not registered)", label.c_str());
				return;
			}

			int current = static_cast<int>(*ptr_);
			if (ImGui::BeginCombo(label.c_str(), names[current].c_str()))
			{
				for (int i = 0; i < names.size(); ++i)
				{
					bool isSelected = (i == current);
					if (ImGui::Selectable(names[i].c_str(), isSelected))
					{
						current = i;
						*ptr_ = static_cast<T>(i);
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}

		// Vector3配列（例：Spline制御点など）
		else if constexpr (std::is_same_v<T, std::vector<Vector3>>)
		{
			ImGui::Text("%s", name.c_str());
			for (size_t i = 0; i < ptr_->size(); ++i)
			{
				std::string pointLabel = "Point " + std::to_string(i) + "##" + uniqueID;
				ImGui::DragFloat3(pointLabel.c_str(), &(*ptr_)[i].x, 0.1f);
			}

			if (ImGui::Button(("Add##" + uniqueID).c_str()))
			{
				ptr_->push_back(Vector3{ 0, 0, 0 });
			}
			ImGui::SameLine();
			if (ImGui::Button(("Clear##" + uniqueID).c_str()))
			{
				ptr_->clear();
			}
		}
#endif // _DEBUG
	}

public:
	///************************* リセット処理 *************************///

	// 登録変数を初期値に戻す
	void ResetValue() override
	{
		*ptr_ = defaultValue_;
	}

private:
	///************************* メンバ変数 *************************///

	T* ptr_;			// 対象変数のポインタ
	T defaultValue_;	// 初期値を保持
};

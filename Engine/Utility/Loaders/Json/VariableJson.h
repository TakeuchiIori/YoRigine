#pragma once
#include <json.hpp>
#include <memory>
#include <string>
#include "ConversionJson.h"

#ifdef _DEBUG
#include <imgui.h>
#endif // _DEBUG

/// <summary>
///  登録変数を抽象化するインターフェース
/// </summary>
class IVariableJson
{
public:
    virtual ~IVariableJson() = default;

    virtual void SaveToJson(nlohmann::json& j) const = 0;
    virtual void LoadFromJson(const nlohmann::json& j) = 0;
    virtual void ShowImGui(const std::string& name, const std::string& uniqueID) = 0;
    virtual void ResetValue() = 0;  // 追加：値をリセットする
};

template <typename T>
class VariableJson : public IVariableJson
{
public:
    /// <summary>
    ///  コンストラクタ
    /// </summary>
    /// <param name="ptr">登録対象の変数ポインタ</param>
    VariableJson(T* ptr)
        : ptr_(ptr)
    {
    }

    /// <summary>
    ///  JSON への保存処理
    /// </summary>
    /// <param name="j">書き込み先 JSON オブジェクト</param>
    void SaveToJson(nlohmann::json& j) const override
    {
        j = *ptr_;
    }

    /// <summary>
    ///  JSON からの読み込み処理
    /// </summary>
    /// <param name="j">読み込み元 JSON オブジェクト</param>
    void LoadFromJson(const nlohmann::json& j) override
    {
        if (!j.is_null())
        {
            *ptr_ = j.get<T>();
        }
    }

    void ShowImGui(const std::string& name, const std::string& uniqueID) override
    {
#ifdef _DEBUG
        std::string label = name + "##" + uniqueID;  // ユニークなIDを付ける

        if constexpr (std::is_same_v<T, int>)
        {
            ImGui::DragInt(label.c_str(), ptr_);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            ImGui::DragFloat(label.c_str(), ptr_);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            ImGui::Checkbox(label.c_str(), ptr_);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            char buffer[256];
            strncpy(buffer, ptr_->c_str(), sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';
            if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
            {
                *ptr_ = std::string(buffer);
            }
        }
        else if constexpr (std::is_same_v<T, Vector2>)
        {
            ImGui::DragFloat2(label.c_str(), reinterpret_cast<float*>(ptr_));
        }
        else if constexpr (std::is_same_v<T, Vector3>)
        {
            ImGui::DragFloat3(label.c_str(), reinterpret_cast<float*>(ptr_));
        }
        else if constexpr (std::is_same_v<T, Quaternion>)
        {
            ImGui::DragFloat4(label.c_str(), reinterpret_cast<float*>(ptr_));
        }
#endif // _DEBUG

    }


    void ResetValue() override
    {
        *ptr_ = defaultValue_;  // 初期値にリセット
    }


private:
    T* ptr_;
    T defaultValue_;  // 初期値を保存
};

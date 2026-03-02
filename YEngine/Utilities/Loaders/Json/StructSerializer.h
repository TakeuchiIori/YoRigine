#pragma once

#include <json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <type_traits>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using json = nlohmann::json;

///************************* 型特性ヘルパー（先に定義） *************************///

// std::vectorかどうか判定
template <typename>
struct IsVector : std::false_type {};

template <typename T, typename Alloc>
struct IsVector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
constexpr bool IsVector_v = IsVector<T>::value;

// x, y, zメンバーを持つか判定（Vector3系）
template <typename, typename = void>
struct HasXYZMembers : std::false_type {};

template <typename T>
struct HasXYZMembers<T, std::void_t<
    decltype(std::declval<T>().x),
    decltype(std::declval<T>().y),
    decltype(std::declval<T>().z)
    >> : std::true_type {};

template <typename T>
constexpr bool HasXYZMembers_v = HasXYZMembers<T>::value;

///************************* 汎用構造体シリアライザー *************************///

template <typename T>
class StructSerializer {
public:
    using Getter = std::function<json(const T&)>;
    using Setter = std::function<void(T&, const json&)>;

#ifdef USE_IMGUI
    using ImGuiDrawer = std::function<bool(T&)>;
#endif

    static StructSerializer<T>& GetInstance() {
        static StructSerializer<T> instance;
        return instance;
    }

    ///************************* フィールド登録（基本型） *************************///

    template <typename FieldType>
    void RegisterField(const std::string& name, FieldType T::* field) {
        getters_[name] = [field](const T& obj) -> json {
            return obj.*field;
            };

        setters_[name] = [field](T& obj, const json& j) {
            if (!j.is_null()) {
                obj.*field = j.get<FieldType>();
            }
            };

#ifdef USE_IMGUI
        RegisterImGuiDrawer<FieldType>(name, field);
#endif
    }

    ///************************* Enum型用の登録 *************************///

    template <typename EnumType>
    void RegisterEnumField(const std::string& name, EnumType T::* field) {
        static_assert(std::is_enum_v<EnumType>, "RegisterEnumField requires enum type");

        getters_[name] = [field](const T& obj) -> json {
            return static_cast<int>(obj.*field);
            };

        setters_[name] = [field](T& obj, const json& j) {
            if (!j.is_null()) {
                obj.*field = static_cast<EnumType>(j.get<int>());
            }
            };

#ifdef USE_IMGUI
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            int current = static_cast<int>(obj.*field);
            bool changed = ImGui::InputInt(name.c_str(), &current);
            if (changed) {
                obj.*field = static_cast<EnumType>(current);
            }
            return changed;
            };
#endif
    }

    ///************************* JSON変換 *************************///

    json ToJson(const T& obj) const {
        json j;
        for (const auto& [name, getter] : getters_) {
            j[name] = getter(obj);
        }
        return j;
    }

    void FromJson(T& obj, const json& j) const {
        for (const auto& [name, setter] : setters_) {
            if (j.contains(name)) {
                setter(obj, j[name]);
            }
        }
    }

#ifdef USE_IMGUI
    bool DrawImGui(T& obj, const std::string& id = "") {
        bool changed = false;

        for (const auto& [name, drawer] : imguiDrawers_) {
            ImGui::PushID((id + "_" + name).c_str());
            if (drawer(obj)) {
                changed = true;
            }
            ImGui::PopID();
        }

        return changed;
    }
#endif

private:
#ifdef USE_IMGUI
    ///************************* ImGui描画関数の自動生成（完全に排他的） *************************///

    // int型
    template <typename FieldType>
    std::enable_if_t<std::is_same_v<FieldType, int>>
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            return ImGui::InputInt(name.c_str(), &(obj.*field));
            };
    }

    // float型
    template <typename FieldType>
    std::enable_if_t<std::is_same_v<FieldType, float>>
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            return ImGui::InputFloat(name.c_str(), &(obj.*field));
            };
    }

    // double型
    template <typename FieldType>
    std::enable_if_t<std::is_same_v<FieldType, double>>
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            float temp = static_cast<float>(obj.*field);
            bool changed = ImGui::InputFloat(name.c_str(), &temp);
            if (changed) obj.*field = static_cast<double>(temp);
            return changed;
            };
    }

    // bool型
    template <typename FieldType>
    std::enable_if_t<std::is_same_v<FieldType, bool>>
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            return ImGui::Checkbox(name.c_str(), &(obj.*field));
            };
    }

    // std::string型
    template <typename FieldType>
    std::enable_if_t<std::is_same_v<FieldType, std::string>>
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            char buffer[256];

            // 安全な文字列コピー
#ifdef _MSC_VER
            strncpy_s(buffer, sizeof(buffer), (obj.*field).c_str(), _TRUNCATE);
#else
            std::strncpy(buffer, (obj.*field).c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
#endif

            bool changed = ImGui::InputText(name.c_str(), buffer, sizeof(buffer));
            if (changed) {
                obj.*field = buffer;
            }
            return changed;
            };
    }

    // std::vector<int>型
    template <typename FieldType>
    std::enable_if_t<
        IsVector_v<FieldType>&&
        std::is_same_v<typename FieldType::value_type, int>
    >
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            bool changed = false;
            if (ImGui::TreeNode(name.c_str())) {
                auto& vec = obj.*field;

                for (size_t i = 0; i < vec.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::InputInt(("##" + std::to_string(i)).c_str(), &vec[i])) {
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        vec.erase(vec.begin() + i);
                        changed = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }

                if (ImGui::Button("Add")) {
                    vec.push_back(0);
                    changed = true;
                }

                ImGui::TreePop();
            }
            return changed;
            };
    }

    // std::vector<Enum>型（Enum配列）
    template <typename FieldType>
    std::enable_if_t<
        IsVector_v<FieldType>&&
        std::is_enum_v<typename FieldType::value_type>
    >
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            using EnumType = typename FieldType::value_type;
            bool changed = false;

            if (ImGui::TreeNode(name.c_str())) {
                auto& vec = obj.*field;

                for (size_t i = 0; i < vec.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));

                    int current = static_cast<int>(vec[i]);
                    if (ImGui::InputInt(("##" + std::to_string(i)).c_str(), &current)) {
                        vec[i] = static_cast<EnumType>(current);
                        changed = true;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        vec.erase(vec.begin() + i);
                        changed = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }

                if (ImGui::Button("Add")) {
                    vec.push_back(static_cast<EnumType>(0));
                    changed = true;
                }

                ImGui::TreePop();
            }
            return changed;
            };
    }

    // Vector3系（x, y, zメンバーを持つ構造体）
    template <typename FieldType>
    std::enable_if_t<
        !std::is_same_v<FieldType, int> &&
        !std::is_same_v<FieldType, float> &&
        !std::is_same_v<FieldType, double> &&
        !std::is_same_v<FieldType, bool> &&
        !std::is_same_v<FieldType, std::string> &&
        !IsVector_v<FieldType>&&
        HasXYZMembers_v<FieldType>
    >
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [field, name](T& obj) -> bool {
            float vec[3] = { (obj.*field).x, (obj.*field).y, (obj.*field).z };
            bool changed = ImGui::InputFloat3(name.c_str(), vec);
            if (changed) {
                (obj.*field).x = vec[0];
                (obj.*field).y = vec[1];
                (obj.*field).z = vec[2];
            }
            return changed;
            };
    }

    // その他の型（デフォルト：表示のみ）
    template <typename FieldType>
    std::enable_if_t<
        !std::is_same_v<FieldType, int> &&
        !std::is_same_v<FieldType, float> &&
        !std::is_same_v<FieldType, double> &&
        !std::is_same_v<FieldType, bool> &&
        !std::is_same_v<FieldType, std::string> &&
        !IsVector_v<FieldType> &&
        !HasXYZMembers_v<FieldType>
    >
        RegisterImGuiDrawer(const std::string& name, FieldType T::* field) {
        imguiDrawers_[name] = [name](T&) -> bool {
            ImGui::Text("%s: (unsupported type)", name.c_str());
            return false;
            };
    }
#endif

private:
    std::unordered_map<std::string, Getter> getters_;
    std::unordered_map<std::string, Setter> setters_;

#ifdef USE_IMGUI
    std::unordered_map<std::string, ImGuiDrawer> imguiDrawers_;
#endif
};

///************************* マクロ定義 *************************///

#define BEGIN_STRUCT_SERIALIZER(StructType) \
    namespace { \
        struct StructType##SerializerInit { \
            StructType##SerializerInit() { \
                auto& serializer = StructSerializer<StructType>::GetInstance();

#define SERIALIZE_FIELD(StructType, FieldName) \
                serializer.RegisterField(#FieldName, &StructType::FieldName);

#define SERIALIZE_ENUM_FIELD(StructType, FieldName) \
                serializer.RegisterEnumField(#FieldName, &StructType::FieldName);

#define END_STRUCT_SERIALIZER(StructType) \
            } \
        } StructType##SerializerInitInstance; \
    } \
    inline void to_json(json& j, const StructType& obj) { \
        j = StructSerializer<StructType>::GetInstance().ToJson(obj); \
    } \
    inline void from_json(const json& j, StructType& obj) { \
        StructSerializer<StructType>::GetInstance().FromJson(obj, j); \
    }
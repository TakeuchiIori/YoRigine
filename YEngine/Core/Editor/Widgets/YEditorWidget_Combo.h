#pragma once
// ===========================================================
// YEditorWidget_Combo.h
//
// コンボボックス系ウィジェット。
// EnumCombo は enum class に特化したテンプレート版。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace YEditorWidget {

// 文字列スパン版コンボ（items は null 終端文字列の配列でもよい）
bool Combo(const char* label, int& currentIndex,
           std::span<const std::string_view> items);

// null 終端 const char* 配列版（既存コードとの互換用）
bool Combo(const char* label, int& currentIndex,
           const char* const* items, int itemCount);

// std::string の値を候補一覧から直接選択するコンボ。
// allowEmpty=true の場合は先頭に「(なし)」を追加する。
bool StringCombo(const char* label, std::string& value,
	std::span<const std::string> items, bool allowEmpty = false);

// ─────────────────────────────────────────────────────────────
// EnumCombo
//
// 使い方:
//   enum class BlendMode { Additive, Alpha, Opaque };
//   static constexpr std::string_view kBlendModeNames[] = {
//       "Additive", "Alpha", "Opaque"
//   };
//   EnumCombo("Blend", blendMode, kBlendModeNames);
//
// ─────────────────────────────────────────────────────────────
template <typename TEnum>
bool EnumCombo(const char* label, TEnum& value,
               std::span<const std::string_view> names)
{
    static_assert(std::is_enum_v<TEnum>, "EnumCombo requires an enum type");
    int idx = static_cast<int>(value);
    if (Combo(label, idx, names)) {
        value = static_cast<TEnum>(idx);
        return true;
    }
    return false;
}

} // namespace YEditorWidget
#endif // USE_IMGUI

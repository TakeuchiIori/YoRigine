#pragma once
// ===========================================================
// YEditorWidget_Input.h  (header-only)
//
// 文字列・真偽値入力の共通ウィジェット。
// バッファサイズをテンプレートで取得し、指定間違いを防ぐ。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include <cstddef>
#include <string>

namespace YEditorWidget {

inline bool Checkbox(const char* label, bool& value)
{
	return ImGui::Checkbox(label, &value);
}

template<std::size_t N>
inline bool InputText(const char* label, char (&buffer)[N],
	ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputText(label, buffer, N, flags);
}

template<std::size_t N>
inline bool InputTextMultiline(const char* label, char (&buffer)[N],
	int visibleLines = 4, ImGuiInputTextFlags flags = 0)
{
	const float height = ImGui::GetTextLineHeight() * static_cast<float>(visibleLines);
	return ImGui::InputTextMultiline(label, buffer, N, ImVec2(-1.0f, height), flags);
}

template<std::size_t N>
inline bool InputPath(const char* label, char (&buffer)[N])
{
	ImGui::SetNextItemWidth(-1.0f);
	return ImGui::InputText(label, buffer, N);
}

namespace Detail {
	struct StringInputUserData {
		std::string* value;
	};

	inline int ResizeStringInput(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
			auto* userData = static_cast<StringInputUserData*>(data->UserData);
			userData->value->resize(static_cast<std::size_t>(data->BufTextLen));
			data->Buf = userData->value->data();
		}
		return 0;
	}
}

inline bool InputText(const char* label, std::string& value,
	ImGuiInputTextFlags flags = 0)
{
	Detail::StringInputUserData userData{ &value };
	return ImGui::InputText(label, value.data(), value.capacity() + 1,
		flags | ImGuiInputTextFlags_CallbackResize, Detail::ResizeStringInput, &userData);
}

inline bool InputTextMultiline(const char* label, std::string& value,
	int visibleLines = 4, ImGuiInputTextFlags flags = 0)
{
	Detail::StringInputUserData userData{ &value };
	const float height = ImGui::GetTextLineHeight() * static_cast<float>(visibleLines);
	return ImGui::InputTextMultiline(label, value.data(), value.capacity() + 1,
		ImVec2(-1.0f, height), flags | ImGuiInputTextFlags_CallbackResize,
		Detail::ResizeStringInput, &userData);
}

} // namespace YEditorWidget
#endif // USE_IMGUI

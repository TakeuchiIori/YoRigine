#pragma once
#include <string>

// ============================================================
// キーフレーム関連の列挙型
// ============================================================
enum class KFChannel {
	Translate,
	Rotate,
	Scale
};

// ============================================================
// 選択中のキーフレーム情報
// ============================================================
struct SelectedKF {
	std::string boneName = "";
	int         index = -1;
	KFChannel   channel = KFChannel::Translate;

	bool IsValid() const { return !boneName.empty() && index >= 0; }
	void Clear() { boneName = ""; index = -1; }
};
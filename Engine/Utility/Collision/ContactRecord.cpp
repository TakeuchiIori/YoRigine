#include "ContactRecord.h"
// C++
#include <algorithm>

void ContactRecord::record(uint32_t number) {
	// 履歴に登録
	history_.push_back(number);
}

// CheckHistory関数
// 指定された番号が履歴に存在するかを確認し、存在すればtrueを返す
bool ContactRecord::CheckHistory(uint32_t number) const {
	// std::any_ofを使用して、history_内にnumberが存在するかを確認
	return std::any_of(history_.begin(), history_.end(), [number](uint32_t n) { return n == number; });
}

void ContactRecord::Clear() {
	// 履歴を抹消
	history_.clear();
}

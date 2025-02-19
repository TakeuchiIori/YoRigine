#pragma once
// C++
#include <vector>
#include "cstdint"
/// <summary>
/// 接触記録
/// </summary>
class ContactRecord {
public:
	// 履歴追加
	void record(uint32_t number);

	// 履歴チェック
	bool CheckHistory(uint32_t number) const;

	// 履歴抹消
	void Clear();

private:
	// 履歴
	std::vector<uint32_t> history_;

};

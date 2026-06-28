#pragma once

// C++
#include <vector>
#include <cstdint>

// 接触履歴を管理するクラス
// 各コライダー同士の接触情報を記録し、重複判定やリセットを行う
class ContactRecord
{
public:
	///************************* 基本関数 *************************///

	// 履歴を追加
	void Record(uint32_t number);

	// 履歴を確認
	bool CheckHistory(uint32_t number) const;

	// 履歴を全削除
	void Clear();

private:
	///************************* メンバ変数 *************************///

	// 接触履歴リスト（コライダー識別番号を保持）
	std::vector<uint32_t> history_;
};

#pragma once

// C++
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <iostream>

// Math
#include "Vector3.h"

///************************* マップチップタイプ列挙 *************************///
/// マップ上で使用されるチップの種類を定義する
enum class MapChipType {
	kBlank,
	kBlock,
};

///************************* マップチップ管理クラス *************************///
/// マップデータの読み込み・取得・判定を行う
class MapChipField {
public:
	///************************* 構造体定義 *************************///
	/// マップチップの二次元データを保持する
	struct MapChipData {
		std::vector<std::vector<MapChipType>> data;
	};

	/// マップチップの境界を表す
	struct Rect {
		float left = 0.0f;
		float right = 1.0f;
		float bottom = 0.0f;
		float top = 1.0f;
	};

	/// マップチップのインデックスセットを表す
	struct IndexSet {
		uint32_t xIndex;
		uint32_t yIndex;
	};

public:
	///************************* コンストラクタ *************************///
	/// 初期化を行う
	MapChipField();

public:
	///************************* マップ管理関数 *************************///
	/// マップデータをリセットする
	void ResetMapChipData();

	/// CSVファイルからマップデータを読み込む
	void LoadMapChipCsv(const std::string& filePath);

	/// 指定位置のマップチップタイプを取得する
	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) const;

	/// 指定インデックスのマップチップの座標を取得する
	static Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);

	/// 指定座標がどのマップチップに属するかを取得する
	IndexSet GetMapChipIndexSetByPosition(const Vector3& position) const;

	/// 指定インデックスのマップチップの矩形を取得する
	Rect GetRectByIndex(uint32_t xIndex, uint32_t yIndex) const;

	/// CSV上の文字キーとマップチップタイプを関連付ける
	void RegisterMapChipType(const std::string& key, MapChipType type);

public:
	///************************* ゲッター *************************///
	/// ブロックサイズを取得する
	float GetBlockSize() const { return blockSize; }

private:
	///************************* メンバ変数 *************************///
	/// マップチップデータ
	MapChipData mapChipData_;

	/// CSVキーとマップタイプの対応表
	std::map<std::string, MapChipType> mapChipTable_;

	/// ブロックサイズ定義
	static inline const float kBlockWidth = 2.0f;
	static inline const float kBlockHeight = 2.0f;
	static inline const float blockSize = 2.0f;

	/// ブロック数定義
	static inline const uint32_t kNumBlockVertical = 20;
	static inline const uint32_t kNumBlockHorizontal = 100;

public:
	///************************* 定数ゲッター *************************///
	/// ブロックの幅を取得する
	static inline float GetBlockWidth() { return kBlockWidth; }

	/// ブロックの高さを取得する
	static inline float GetBlockHeight() { return kBlockHeight; }

	/// 縦方向のブロック数を取得する
	static inline uint32_t GetNumBlockVertical() { return kNumBlockVertical; }

	/// 横方向のブロック数を取得する
	static inline uint32_t GetNumBlockHorizontal() { return kNumBlockHorizontal; }
};

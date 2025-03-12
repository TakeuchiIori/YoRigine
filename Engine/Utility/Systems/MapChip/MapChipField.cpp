#include "MapChipField.h"

MapChipField::MapChipField() {
    // デフォルトのマップチップタイプを登録
    RegisterMapChipType("0", MapChipType::kBlank);
    RegisterMapChipType("1", MapChipType::kBlock);

    // マップチップデータの初期化
    ResetMapChipData();
}

void MapChipField::ResetMapChipData() {
    // より効率的な初期化方法
    mapChipData_.data.clear();
    mapChipData_.data.resize(kNumBlockVertical,
        std::vector<MapChipType>(kNumBlockHorizontal, MapChipType::kBlank));
}

void MapChipField::RegisterMapChipType(const std::string& key, MapChipType type) {
    mapChipTable_[key] = type;
}

void MapChipField::LoadMapChipCsv(const std::string& filePath) {
    // マップチップデータをリセット
    ResetMapChipData();

    // ファイルを開く
    std::ifstream file(filePath);
    // 元のコードと同様に、ファイルが開けなかった場合はassertでエラーを検出
    assert(file.is_open());

    // 行番号（デバッグ用）
    uint32_t lineNumber = 0;

    // ファイルの各行を処理
    std::string line;
    while (getline(file, line) && lineNumber < kNumBlockVertical) {
        // 空行をスキップ
        if (line.empty()) {
            continue;
        }

        // 1行分の文字列をストリームに変換
        std::istringstream line_stream(line);

        // 列番号
        uint32_t columnNumber = 0;

        // 行の各要素を処理
        std::string word;
        while (getline(line_stream, word, ',') && columnNumber < kNumBlockHorizontal) {
            // 空白を削除
            word.erase(std::remove_if(word.begin(), word.end(), isspace), word.end());

            // 有効なキーかチェック
            if (!word.empty() && mapChipTable_.count(word) > 0) {
                mapChipData_.data[lineNumber][columnNumber] = mapChipTable_[word];
            } else {
                // 不明なキーはデフォルト値を使用
                mapChipData_.data[lineNumber][columnNumber] = MapChipType::kBlank;
            }

            ++columnNumber;
        }

        // 残りの列をデフォルト値で埋める
        while (columnNumber < kNumBlockHorizontal) {
            mapChipData_.data[lineNumber][columnNumber] = MapChipType::kBlank;
            ++columnNumber;
        }

        ++lineNumber;
    }

    // 残りの行をデフォルト値で埋める
    while (lineNumber < kNumBlockVertical) {
        for (uint32_t j = 0; j < kNumBlockHorizontal; ++j) {
            mapChipData_.data[lineNumber][j] = MapChipType::kBlank;
        }
        ++lineNumber;
    }

    file.close();
}

MapChipType MapChipField::GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) const {
    // 符号なし整数なので、負の値チェックは不要
    if (xIndex >= kNumBlockHorizontal || yIndex >= kNumBlockVertical) {
        return MapChipType::kBlank;
    }

    return mapChipData_.data[yIndex][xIndex];
}

Vector3 MapChipField::GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) {
    return Vector3(
        kBlockWidth * xIndex,
        kBlockHeight * (kNumBlockVertical - 1 - yIndex),
        0
    );
}

MapChipField::IndexSet MapChipField::GetMapChipIndexSetByPosition(const Vector3& position) const {
    IndexSet indexSet = {};
    indexSet.xIndex = static_cast<uint32_t>((position.x + kBlockWidth / 2) / kBlockWidth);
    indexSet.yIndex = kNumBlockVertical - 1 - static_cast<uint32_t>((position.y + kBlockHeight / 2) / kBlockHeight);

    // 範囲外チェック
    if (indexSet.xIndex >= kNumBlockHorizontal) {
        indexSet.xIndex = kNumBlockHorizontal - 1;
    }
    if (indexSet.yIndex >= kNumBlockVertical) {
        indexSet.yIndex = kNumBlockVertical - 1;
    }

    return indexSet;
}

MapChipField::Rect MapChipField::GetRectByIndex(uint32_t xIndex, uint32_t yIndex) const {
    // 指定ブロックの中心点座標を取得する
    Vector3 center = GetMapChipPositionByIndex(xIndex, yIndex);

    Rect rect;
    rect.left   = center.x - kBlockWidth / 2.0f;
    rect.right  = center.x + kBlockWidth / 2.0f;
    rect.bottom = center.y - kBlockHeight / 2.0f;
    rect.top    = center.y + kBlockHeight / 2.0f;
    return rect;
}
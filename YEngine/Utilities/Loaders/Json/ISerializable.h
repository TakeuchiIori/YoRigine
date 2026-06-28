#pragma once
#include <json.hpp>
#include <string>

/// <summary>
/// シリアライゼーション可能なオブジェクトのインターフェース
/// JSON形式でのデータ保存・読み込みを提供
/// </summary>
class ISerializable {
public:
    virtual ~ISerializable() = default;

    /// <summary>
    /// オブジェクトの状態をJSONに保存
    /// </summary>
    /// <param name="json">書き込み先のJSONオブジェクト</param>
    virtual void SaveToJson(nlohmann::json& json) const = 0;

    /// <summary>
    /// JSONからオブジェクトの状態を読み込み
    /// </summary>
    /// <param name="json">読み込み元のJSONオブジェクト</param>
    virtual void LoadFromJson(const nlohmann::json& json) = 0;

    /// <summary>
    /// オブジェクトのタイプ名を取得
    /// ファクトリーパターンで実体を生成する際に使用
    /// </summary>
    /// <returns>タイプ名（例: "SpawnVelocity", "UpdateGravity"）</returns>
    virtual std::string GetTypeName() const = 0;
};
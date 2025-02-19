#pragma once

// C++
#include <string>
#include <map>
#include <memory>

// Engine
#include "Model.h"
#include "ModelCommon.h"
#include "DX./DirectXCommon.h"

class ModelManager
{
public: // シングルトン
    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static ModelManager* GetInstance();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    // コンストラクタ
    ModelManager() = default;
    ~ModelManager() = default;

public: // 公開メンバ関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dxCommon"></param>
    void Initialze(DirectXCommon* dxCommon);

    /// <summary>
    /// モデルファイルの読み込み
    /// </summary>
    /// <param name="filePath"></param>
    void LoadModel(const std::string& directoryPath,const std::string& filePath,bool isAnimation = false);

    /// <summary>
    /// モデルの検索
    /// </summary>
    /// <param name="filePath"></param>
    /// <returns></returns>
    Model* FindModel(const std::string& filePath);

private:
    // シングルトンインスタンス
    static std::unique_ptr<ModelManager> instance;
    static std::once_flag initInstanceFlag;

    // コピーコンストラクタと代入演算子を削除
    ModelManager(ModelManager&) = delete;
    ModelManager& operator=(ModelManager&) = delete;

    // モデルデータの格納用マップ（モデルのファイルパスをキーとしたユニークポインタ）
    std::map<std::string, std::unique_ptr<Model>> models;

private: // メンバ変数
    // モデル共通部分
    std::unique_ptr<ModelCommon> modelCommon_;
};

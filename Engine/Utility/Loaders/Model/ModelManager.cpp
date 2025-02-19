#include "ModelManager.h"

// シングルトンインスタンスの初期化
std::unique_ptr<ModelManager> ModelManager::instance = nullptr;
std::once_flag ModelManager::initInstanceFlag;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
ModelManager* ModelManager::GetInstance()
{
    std::call_once(initInstanceFlag, []() {
        instance = std::make_unique<ModelManager>();
        });
    return instance.get();
}

/// <summary>
/// 初期化処理
/// </summary>
/// <param name="dxCommon">DirectXの共通オブジェクト</param>
void ModelManager::Initialze(DirectXCommon* dxCommon)
{
    // ModelCommonのインスタンスを生成し、初期化
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon);
}

/// <summary>
/// モデルファイルの読み込み
/// </summary>
/// <param name="filePath">読み込むモデルのファイルパス</param>
void ModelManager::LoadModel(const std::string& directoryPath,const std::string& filePath ,bool isAnimation)
{
    // 読み込まれているモデルを検索
    if (models.contains(filePath)) {
        // すでに読み込み済みであれば何もせず終了
        return;
    }

    // 新しいモデルの生成、ファイル読み込み、初期化
    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), directoryPath, filePath, isAnimation);

    // モデルをマップに格納（所有権を譲渡）
    models.insert(std::make_pair(filePath, std::move(model)));
}

/// <summary>
/// モデルの検索
/// </summary>
/// <param name="filePath">検索するモデルのファイルパス</param>
/// <returns>モデルが見つかった場合、そのポインタ。見つからなければnullptr。</returns>
Model* ModelManager::FindModel(const std::string& filePath)
{
    // 読み込まれているモデルを検索
    if (models.contains(filePath)) {
        // 見つかった場合、そのモデルを返す
        return models.at(filePath).get();
    }
    // モデルが見つからない場合はnullptrを返す
    return nullptr;
}

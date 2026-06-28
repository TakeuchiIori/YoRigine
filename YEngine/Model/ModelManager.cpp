#include "ModelManager.h"
#include "Debugger/Logger.h"

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
void ModelManager::Initialze(YoRigine::DirectXCommon* dxCommon)
{
	// ModelCommonのインスタンスを生成し、初期化
	ModelCommon::GetInstance()->Initialize(dxCommon);
}

/// <summary>
/// モデルファイルの読み込み
/// </summary>
/// <param name="filePath">読み込むモデルのファイルパス</param>
void ModelManager::LoadModel(const std::string& directoryPath, const std::string& filePath, const std::string& animationName, bool isAnimation)
{
	// アニメーション名を含んだユニークキーを生成
	std::string modelKey = filePath;
	if (isAnimation) {
		modelKey += "#" + animationName;
	}

	// 読み込まれているモデルを検索
	if (models.contains(modelKey)) {
		return;
	}

	// モデル生成と初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(ModelCommon::GetInstance(), directoryPath, filePath, animationName, isAnimation);
	model->SetName(filePath);
	// 登録
	models.insert(std::make_pair(modelKey, std::move(model)));
}

/// <summary>
/// モデルの検索
/// </summary>
/// <param name="filePath">検索するモデルのファイルパス</param>
/// <returns>モデルが見つかった場合、そのポインタ。見つからなければnullptr。</returns>
Model* ModelManager::FindModel(const std::string& filePath, const std::string& animationName, bool isAnimation)
{
	std::string modelKey = filePath;
	if (isAnimation) {
		modelKey += "#" + animationName;
	}

	if (models.contains(modelKey)) {
		return models.at(modelKey).get();
	}
	return nullptr;
}

/// <summary>
/// パスから「拡張子なしのベース名」と「拡張子付きファイル名」を分離する
/// 例: "Enemy/Slime.obj"
///   → base: "Enemy/Slime"
///   → base: "Enemy/Slime"
///   → file: "Slime.obj"
/// </summary>
std::pair<std::string, std::string> ModelManager::ParseModelPath(const std::string& filePath)
{
	std::string base;
	std::string file;

	// 拡張子チェック
	if (filePath.ends_with(".obj")) {
		base = filePath.substr(0, filePath.size() - 4);
		file = base.substr(base.find_last_of("/\\") + 1) + ".obj";
	} else if (filePath.ends_with(".gltf")) {
		base = filePath.substr(0, filePath.size() - 5);
		file = base.substr(base.find_last_of("/\\") + 1) + ".gltf";
	} else {
		// 未対応拡張子の場合
		ThrowError("未対応のModelPath拡張子です。");
	}

	return { base, file };
}

std::vector<std::string> ModelManager::GetModelKeys() const
{
	std::vector<std::string> keys;
	keys.reserve(models.size());

	for (const auto& [key, model] : models) {
		keys.push_back(key);
	}
	return keys;
}

std::vector<Model*> ModelManager::GetAllModels() const
{
	std::vector<Model*> list;
	list.reserve(models.size());

	for (const auto& [key, model] : models) {
		list.push_back(model.get());
	}
	return list;
}


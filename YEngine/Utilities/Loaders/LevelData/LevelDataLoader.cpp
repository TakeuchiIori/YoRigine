#include "LevelDataLoader.h"
#include "Model.h"
#include "ModelManager.h"

const std::string LevelDataLoader::defaultPath = "Resources/Json/LevelData/";
const std::string LevelDataLoader::defaultFileName = "t.json";
const std::string LevelDataLoader::defaultModelPath_ = "Resources/Models/";

void LevelDataLoader::Initialize()
{
	FileCheck();
	ScanningObjects();
	SetScene();
}

void LevelDataLoader::FileCheck()
{
	const  std::string fullPath = defaultPath + defaultFileName;
	std::ifstream file;

	file.open(fullPath);

	if (file.fail()) {
		assert(0);
	}
	// 解答
	file >> deserialized_;

	// 正しいレベルデータファイルかチェック
	assert(deserialized_.is_object());
	assert(deserialized_.contains("name"));
	assert(deserialized_["name"].is_string());

	// name を文字列として取得
	std::string name = deserialized_["name"].get<std::string>();

	// 正しいレベルデータファイルかチェック
	assert(name.compare("scene") == 0);

}

void LevelDataLoader::ScanningObjects()
{

	levelData_ = std::make_unique<LevelData>();

	// objects の全オブジェクト走査
	for (nlohmann::json& object : deserialized_["objects"]) {
		assert(object.contains("type"));

		// 種別を取得
		std::string type = object["type"].get<std::string>();


		/*----------------------------------------------------------------------------


								 ここからオブジェクトの情報読み込み


		-----------------------------------------------------------------------------*/

		/// Mesh
		if (type.compare("MESH") == 0) {
			// 要素追加
			levelData_->objData.emplace_back(ObjectData{});
			ObjectData& objectData = levelData_->objData.back();

			if (object.contains("file_name")) {
				objectData.fileName = object["file_name"];
			}
			/// Transform
			nlohmann::json transform = object["transform"];

			// Translate
			objectData.translation.x = static_cast<float>(transform["translation"][0]);
			objectData.translation.y = static_cast<float>(transform["translation"][1]);
			objectData.translation.z = static_cast<float>(transform["translation"][2]);

			// Rotation
			objectData.rotation.x = static_cast<float>(transform["rotation"][0]);
			objectData.rotation.y = static_cast<float>(transform["rotation"][1]);
			objectData.rotation.z = static_cast<float>(transform["rotation"][2]);

			// Scale
			objectData.scale.x = static_cast<float>(transform["scaling"][0]);
			objectData.scale.y = static_cast<float>(transform["scaling"][1]);
			objectData.scale.z = static_cast<float>(transform["scaling"][2]);


			/// オブジェクト走査を再起関数にまとめ、再起的に呼び出す
			if (object.contains("children")) {
				for (const auto& child : object["children"]) {
					ObjectData childData;
					LoadObjectRecursive(child, childData);
					objectData.children.push_back(std::move(childData));
				}
			}



		}
	}
}

void LevelDataLoader::LoadObjectRecursive(const nlohmann::json& jsonObject, ObjectData& parent)
{
	// 共通の読み込み処理
	if (jsonObject.contains("file_name")) {
		parent.fileName = jsonObject["file_name"];
	}
	if (jsonObject.contains("transform")) {
		auto& t = jsonObject["transform"];

		if (t.contains("translation") && t["translation"].is_array() && t["translation"].size() == 3) {
			parent.translation.x = t["translation"][0];
			parent.translation.y = t["translation"][1];
			parent.translation.z = t["translation"][2];
		}
		if (t.contains("rotation") && t["rotation"].is_array() && t["rotation"].size() == 3) {
			parent.rotation.x = t["rotation"][0];
			parent.rotation.y = t["rotation"][1];
			parent.rotation.z = t["rotation"][2];
		}
		if (t.contains("scaling") && t["scaling"].is_array() && t["scaling"].size() == 3) {
			parent.scale.x = t["scaling"][0];
			parent.scale.y = t["scaling"][1];
			parent.scale.z = t["scaling"][2];
		}
	}

	if (jsonObject.contains("collider")) {
		auto& c = jsonObject["collider"];

		if (c.contains("type")) {
			parent.colliderType = c["type"];
		}
		if (c.contains("center") && c["center"].is_array() && c["center"].size() == 3) {
			parent.colliderCenter.x = c["center"][0];
			parent.colliderCenter.y = c["center"][1];
			parent.colliderCenter.z = c["center"][2];
		}
		if (c.contains("size") && c["size"].is_array() && c["size"].size() == 3) {
			parent.colliderSize.x = c["size"][0];
			parent.colliderSize.y = c["size"][1];
			parent.colliderSize.z = c["size"][2];
		}
	}

	// childrenがある場合は再帰的に処理
	if (jsonObject.contains("children") && jsonObject["children"].is_array()) {
		for (const auto& childJson : jsonObject["children"]) {
			ObjectData child;
			LoadObjectRecursive(childJson, child);
			parent.children.push_back(std::move(child));
		}
	}
}

void LevelDataLoader::SetScene()
{
	for (auto& data : levelData_->objData)
	{
		auto obj = Object3d::Create(data.fileName);
		if (!obj) continue;

		auto wt = std::make_unique<WorldTransform>();
		wt->Initialize();
		wt->translate_ = ConvertPosition(data.translation);
		wt->rotate_ = data.rotation;
		wt->scale_ = data.scale;

		objects_.push_back(std::move(obj));
		worldTransforms_.push_back(std::move(wt));
	}
}


void LevelDataLoader::Update()
{
	for (size_t i = 0; i < worldTransforms_.size(); ++i) {
		worldTransforms_[i]->UpdateMatrix();
	}
}

void LevelDataLoader::Draw(Camera* camera)
{
	for (size_t i = 0; i < objects_.size(); ++i) {
		objects_[i]->Draw(camera, *worldTransforms_[i]);
	}
}



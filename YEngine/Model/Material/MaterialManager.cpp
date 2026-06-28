#include "MaterialManager.h"


MaterialManager* MaterialManager::instance_ = nullptr;

MaterialManager* MaterialManager::GetInstance() {
	if (!instance_) {
		instance_ = new MaterialManager();
	}
	return instance_;
}

//uint32_t MaterialManager::LoadMaterial(const std::string& name, std::shared_ptr<Material> material) {
//	auto it = nameToIndex_.find(name);
//	if (it != nameToIndex_.end()) {
//		return it->second; // 既にある
//	}
//	uint32_t index = static_cast<uint32_t>(materials_.size());
//	materials_.push_back(material);
//	nameToIndex_[name] = index;
//	return index;
//}
//
//uint32_t MaterialManager::LoadMaterial(const std::string& name) {
//	auto it = nameToIndex_.find(name);
//	if (it != nameToIndex_.end()) {
//		return it->second; // 既にある
//	}
//
//	std::shared_ptr<Material> material = std::make_shared<Material>();
//	material->SetName(name);
//	material->SetTextureFilePath(name + ".png"); // 仮：名前＋.png をテクスチャとして想定
//	material->Initialize();
//
//	return LoadMaterial(name, material);
//}

Material* MaterialManager::GetMaterial(uint32_t index) {
	if (index < materials_.size()) {
		return materials_[index].get();
	}
	return nullptr;
}

Material* MaterialManager::GetMaterialByName(const std::string& name) {
	auto it = nameToIndex_.find(name);
	if (it != nameToIndex_.end()) {
		return materials_[it->second].get();
	}
	return nullptr;
}

const std::vector<std::shared_ptr<Material>>& MaterialManager::GetAllMaterials() const {
	return materials_;
}

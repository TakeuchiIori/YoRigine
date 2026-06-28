#pragma once

#include "Material.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <assimp/material.h>

// マテリアル管理クラス
class MaterialManager {
public:

	///************************* 基本関数 *************************///
	static MaterialManager* GetInstance();


	///************************* アクセッサ *************************///

	// インデックスからマテリアル取得
	Material* GetMaterial(uint32_t index);

	// 名前からマテリアル取得（存在しなければnullptr）
	Material* GetMaterialByName(const std::string& name);

	// 全マテリアル取得
	const std::vector<std::shared_ptr<Material>>& GetAllMaterials() const;

private:

	///************************* メンバ変数 *************************///
	static MaterialManager* instance_;

	// 名前→インデックスマップ
	std::unordered_map<std::string, uint32_t> nameToIndex_;

	// マテリアル配列
	std::vector<std::shared_ptr<Material>> materials_;

};
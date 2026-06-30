#include "UIManager.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <set>

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Loaders/Texture/TextureManager.h>
#include <imgui_internal.h>

namespace YoRigine {
	const std::string UIManager::SCENE_DIRECTORY = "./Resources/UIScenes/";
	const std::string UIManager::UI_CONFIG_DIRECTORY = "./Resources/UIConfigs/";

	UIManager* UIManager::GetInstance() {
		static UIManager instance;
		return &instance;
	}

	/*==================================================================
							UI基本管理
	===================================================================*/

	void UIManager::AddUI(const std::string& id, std::unique_ptr<UIBase> ui) {
		if (!ui) return;

		uiElements_[id] = std::move(ui);
		RebuildDrawOrder();  // 追加時に描画順序を更新
	}

	void UIManager::BringToFront(const std::string& id) {
		auto it = std::find(drawOrder_.begin(), drawOrder_.end(), id);
		if (it == drawOrder_.end()) return;
		std::string s = *it;
		drawOrder_.erase(it);
		drawOrder_.push_back(s);  // 最後尾＝同レイヤー内で最後に描画＝最前面
	}

	// RemoveUI()でも同様
	void UIManager::RemoveUI(const std::string& id) {
		for (auto& [groupName, uiIds] : groups_) {
			auto it = std::find(uiIds.begin(), uiIds.end(), id);
			if (it != uiIds.end()) {
				uiIds.erase(it);
			}
		}

		uiElements_.erase(id);
		RebuildDrawOrder();  // 削除時に描画順序を更新
	}

	UIBase* UIManager::GetUI(const std::string& id) {
		auto it = uiElements_.find(id);
		if (it != uiElements_.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	bool UIManager::HasUI(const std::string& id) const {
		return uiElements_.find(id) != uiElements_.end();
	}

	void UIManager::Clear() {
		uiElements_.clear();
		groups_.clear();
		drawOrder_.clear();
		selectedUIId_.clear();
	}

	/// <summary>
	///  UIのIDを変更（重複時は連番付与）
	/// </summary>
	/// <param name="oldId">現在のID</param>
	/// <param name="newId">変更したいID</param>
	/// <returns>成功時true</returns>
	bool UIManager::RenameUI(const std::string& oldId, const std::string& newId) {
		if (oldId == newId) return true;
		auto it = uiElements_.find(oldId);
		if (it == uiElements_.end()) return false;

		// 空や空白のみのIDは不可
		auto trim = [](std::string s) {
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
			return s;
			};
		std::string base = trim(newId);
		if (base.empty()) return false;

		// 重複回避
		std::string finalId = base;
		int counter = 1;
		while (HasUI(finalId)) {
			finalId = base + "_" + std::to_string(counter++);
		}

		// 要素をムーブして差し替え
		auto uiPtr = std::move(it->second);
		uiElements_.erase(it);
		uiElements_.emplace(finalId, std::move(uiPtr));

		// グループ参照を更新
		for (auto& [gname, ids] : groups_) {
			for (auto& idRef : ids) {
				if (idRef == oldId) idRef = finalId;
			}
		}

		// 描画順を更新
		for (auto& idInOrder : drawOrder_) {
			if (idInOrder == oldId) idInOrder = finalId;
		}

		// 選択ID更新
		if (selectedUIId_ == oldId) selectedUIId_ = finalId;

		// 念のため順序再構築
		RebuildDrawOrder();
		return true;
	}


	/*==================================================================
							一括更新・描画
	===================================================================*/

	void UIManager::UpdateAll() {
		for (auto& [id, ui] : uiElements_) {
			if (ui) {
				ui->Update();
			}
		}
	}

	void UIManager::DrawAll() {
		// drawOrder_をレイヤーごとにグループ化して描画
		std::map<int, std::vector<std::string>> layerGroups;

		// drawOrder_の順序を維持しながらレイヤーごとに分類
		for (const auto& id : drawOrder_) {
			auto it = uiElements_.find(id);
			if (it != uiElements_.end() && it->second) {
				int layer = it->second->GetLayer();
				layerGroups[layer].push_back(id);
			}
		}

		// レイヤー順(小→大)に描画、各レイヤー内はdrawOrder_の順序を維持
		for (const auto& [layer, uiIds] : layerGroups) {
			for (const auto& id : uiIds) {
				auto it = uiElements_.find(id);
				if (it != uiElements_.end() && it->second) {
					it->second->Draw();
				}
			}
		}
	}

	void UIManager::Draw(const std::string& id) {
		auto it = uiElements_.find(id);
		if (it != uiElements_.end() && it->second) {
			it->second->Draw();
		}
	}

	/*==================================================================
							レイヤー管理
	===================================================================*/

	void UIManager::SortByLayer() {
		RebuildDrawOrder();
	}

	void UIManager::ShowLayer(int layer, bool show) {
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetLayer() == layer) {
				ui->SetVisible(show);
			}
		}
	}

	void UIManager::ShowAll(bool show) {
		for (auto& [id, ui] : uiElements_) {
			if (ui) {
				ui->SetVisible(show);
			}
		}
	}

	std::vector<UIBase*> UIManager::GetUIsByLayer(int layer) {
		std::vector<UIBase*> result;
		// 描画順に基づいてUIを収集
		for (const auto& id : drawOrder_) {
			auto it = uiElements_.find(id);
			if (it != uiElements_.end() && it->second && it->second->GetLayer() == layer) {
				result.push_back(it->second.get());
			}
		}
		return result;
	}

	/*==================================================================
							シーン管理
	===================================================================*/

	bool UIManager::SaveScene(const std::string& sceneName) {
		if (sceneName.empty()) return false;

		try {
			// 必要なフォルダを作成
			std::filesystem::create_directories(SCENE_DIRECTORY);
			std::filesystem::create_directories(UI_CONFIG_DIRECTORY);
			const std::string sceneConfigDir = GetSceneConfigDir(sceneName);
			std::filesystem::create_directories(sceneConfigDir);

			nlohmann::json sceneData;
			nlohmann::json uiArray = nlohmann::json::array();

			// drawOrder_の順番でUIを保存
			for (const auto& id : drawOrder_) {
				auto it = uiElements_.find(id);
				if (it == uiElements_.end() || !it->second) continue;

				nlohmann::json uiData;
				uiData["id"] = id;

				// UIごとの設定を シーン名フォルダ/ID.json に保存
				uiData["configPath"] = sceneConfigDir + id + ".json";
				it->second->SaveToJSON(uiData["configPath"]);

				uiArray.push_back(uiData);
			}
			sceneData["uis"] = uiArray;

			// 描画順序を保存
			sceneData["drawOrder"] = drawOrder_;

			// グループ情報（そのまま）
			nlohmann::json groupsData;
			for (const auto& [groupName, uiIds] : groups_) {
				groupsData[groupName] = uiIds;
			}
			sceneData["groups"] = groupsData;

			// シーンJSONを書き出し
			const std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
			std::ofstream file(scenePath);
			if (!file.is_open()) return false;

			file << std::setw(4) << sceneData << std::endl;
			file.close();
			this->currentSceneName_ = sceneName;
			return true;
		}
		catch (const std::exception& e) {
			printf("シーン保存エラー: %s\n", e.what());
			return false;
		}
	}

	bool UIManager::LoadScene(const std::string& sceneName) {
		const std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
		std::ifstream f(scenePath);
		if (f.is_open()) {
			this->currentSceneName_ = sceneName;;
		}
		else {
			return false;
		}

		nlohmann::json sceneData;
		f >> sceneData;
		f.close();

		// 既存UIをクリア
		Clear();

		const std::string sceneConfigDir = GetSceneConfigDir(sceneName);

		// UI 群の復元
		if (sceneData.contains("uis")) {
			for (const auto& uiEntry : sceneData["uis"]) {
				const std::string id = uiEntry.value("id", "");
				if (id.empty()) continue;

				// シーンJSONに書かれた configPath を最優先
				std::string cfgPath = uiEntry.value("configPath", "");

				// 無ければ新形式 <UIConfigs>/<SceneName>/<ID>.json
				if (cfgPath.empty()) {
					cfgPath = sceneConfigDir + id + ".json";
				}

				// 無ければ旧形式 <UIConfigs>/<ID>.json（後方互換）
				if (!std::filesystem::exists(cfgPath)) {
					const std::string legacy = UI_CONFIG_DIRECTORY + id + ".json";
					if (std::filesystem::exists(legacy)) {
						cfgPath = legacy;
					}
				}

				// 実体生成 - 直接uiElements_に追加してRebuildDrawOrderを回避
				auto ui = std::make_unique<UIBase>(id);
				ui->Initialize(cfgPath);
				uiElements_[id] = std::move(ui);
			}
		}

		// 描画順序の復元（保存されていれば優先、無ければレイヤー順で再構築）
		if (sceneData.contains("drawOrder")) {
			drawOrder_.clear();
			for (const auto& id : sceneData["drawOrder"]) {
				std::string idStr = id.get<std::string>();
				// 実際に存在するUIのみ追加
				if (uiElements_.find(idStr) != uiElements_.end()) {
					drawOrder_.push_back(idStr);
				}
			}
		} else {
			// 後方互換性：drawOrderが無い場合はレイヤー順で再構築
			RebuildDrawOrder();
		}

		// グループの復元（既存のままでOK）
		if (sceneData.contains("groups")) {
			for (auto it = sceneData["groups"].begin(); it != sceneData["groups"].end(); ++it) {
				const std::string groupName = it.key();
				for (const auto& id : it.value()) {
					AddToGroup(groupName, id.get<std::string>());
				}
			}
		}

		return true;
	}


	std::vector<std::string> UIManager::GetAvailableScenes() const {
		std::vector<std::string> scenes;

		if (!std::filesystem::exists(SCENE_DIRECTORY)) {
			return scenes;
		}

		for (const auto& entry : std::filesystem::directory_iterator(SCENE_DIRECTORY)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				std::string filename = entry.path().stem().string();
				if (filename.find("temp_") != 0) {
					scenes.push_back(filename);
				}
			}
		}

		std::sort(scenes.begin(), scenes.end());
		return scenes;
	}

	bool UIManager::DeleteScene(const std::string& sceneName) {
		if (sceneName.empty()) return false;

		try {
			std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
			if (std::filesystem::exists(scenePath)) {
				return std::filesystem::remove(scenePath);
			}
			return false;
		}
		catch (const std::exception& e) {
			printf("シーン削除エラー: %s\n", e.what());
			return false;
		}
	}

	/*==================================================================
							グループ管理
	===================================================================*/

	void UIManager::AddToGroup(const std::string& groupName, const std::string& uiId) {
		if (!HasUI(uiId)) return;

		auto& group = groups_[groupName];
		if (std::find(group.begin(), group.end(), uiId) == group.end()) {
			group.push_back(uiId);
		}
	}

	void UIManager::RemoveFromGroup(const std::string& groupName, const std::string& uiId) {
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			auto& group = it->second;
			group.erase(std::remove(group.begin(), group.end(), uiId), group.end());

			if (group.empty()) {
				groups_.erase(it);
			}
		}
	}

	void UIManager::ShowGroup(const std::string& groupName, bool show) {
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			for (const auto& uiId : it->second) {
				auto* ui = GetUI(uiId);
				if (ui) {
					ui->SetVisible(show);
				}
			}
		}
	}

	std::vector<UIBase*> UIManager::GetGroup(const std::string& groupName) {
		std::vector<UIBase*> result;
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			for (const auto& uiId : it->second) {
				auto* ui = GetUI(uiId);
				if (ui) {
					result.push_back(ui);
				}
			}
		}
		return result;
	}

	/*==================================================================
							検索・フィルタ
	===================================================================*/

	std::vector<UIBase*> UIManager::FindByName(const std::string& name) {
		std::vector<UIBase*> result;
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetName().find(name) != std::string::npos) {
				result.push_back(ui.get());
			}
		}
		return result;
	}

	std::vector<UIBase*> UIManager::FindByTexture(const std::string& texturePath) {
		std::vector<UIBase*> result;
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetTexturePath() == texturePath) {
				result.push_back(ui.get());
			}
		}
		return result;
	}

	/*==================================================================
							統計情報
	===================================================================*/

	UIManager::Statistics UIManager::GetStatistics() const {
		Statistics stats;
		stats.totalUIs = static_cast<int>(uiElements_.size());

		for (const auto& [id, ui] : uiElements_) {
			if (!ui) continue;

			if (ui->IsVisible()) {
				stats.visibleUIs++;
			} else {
				stats.hiddenUIs++;
			}

			int layer = ui->GetLayer();
			stats.uisByLayer[layer]++;
		}

		return stats;
	}

	// ============================================================
	// ImGui エディタ (ImGuiDebug / DisplayImprovedTextureSelector /
	// DisplayTextureDirectory) は UIManagerEditor.cpp に分離した。
	// ============================================================


	void UIManager::RebuildDrawOrder() {
		// 1. uiElements_にあってdrawOrder_にないIDを追加(新規UI対応)
		for (const auto& [id, ui] : uiElements_) {
			if (std::find(drawOrder_.begin(), drawOrder_.end(), id) == drawOrder_.end()) {
				drawOrder_.push_back(id);
			}
		}

		// 2. drawOrder_にあってuiElements_にないIDを削除(削除済みUI対応)
		drawOrder_.erase(std::remove_if(drawOrder_.begin(), drawOrder_.end(),
			[this](const std::string& id) { return uiElements_.find(id) == uiElements_.end(); }),
			drawOrder_.end());

		// 3. レイヤー順でstable_sort(同一レイヤー内の相対順序=drawOrder_内の順序を維持)
		std::stable_sort(drawOrder_.begin(), drawOrder_.end(),
			[this](const std::string& a, const std::string& b) {
				auto itA = uiElements_.find(a);
				auto itB = uiElements_.find(b);
				if (itA == uiElements_.end() || itB == uiElements_.end()) return false;

				int layerA = itA->second->GetLayer();
				int layerB = itB->second->GetLayer();
				return layerA < layerB;
			});
	}

	void UIManager::MoveDrawOrderForward(const std::string& uiId) {
		auto it = std::find(drawOrder_.begin(), drawOrder_.end(), uiId);
		if (it != drawOrder_.end() && it != drawOrder_.begin()) {
			// 同じレイヤー内でのみ移動を許可
			auto prevIt = it - 1;
			auto curUi = uiElements_.find(*it);
			auto prevUi = uiElements_.find(*prevIt);

			if (curUi != uiElements_.end() && prevUi != uiElements_.end() &&
				curUi->second && prevUi->second &&
				curUi->second->GetLayer() == prevUi->second->GetLayer()) {
				std::iter_swap(it, prevIt);
			}
		}
	}

	void UIManager::MoveDrawOrderBackward(const std::string& uiId) {
		auto it = std::find(drawOrder_.begin(), drawOrder_.end(), uiId);
		if (it != drawOrder_.end() && it + 1 != drawOrder_.end()) {
			// 同じレイヤー内でのみ移動を許可
			auto nextIt = it + 1;
			auto curUi = uiElements_.find(*it);
			auto nextUi = uiElements_.find(*nextIt);

			if (curUi != uiElements_.end() && nextUi != uiElements_.end() &&
				curUi->second && nextUi->second &&
				curUi->second->GetLayer() == nextUi->second->GetLayer()) {
				std::iter_swap(it, nextIt);
			}
		}
	}

	std::string UIManager::GenerateUniqueID(const std::string& baseName) {
		std::string id = baseName;
		int counter = 1;

		while (HasUI(id)) {
			id = baseName + "_" + std::to_string(counter);
			counter++;
		}

		return id;
	}

	std::string UIManager::GetSceneConfigDir(const std::string& sceneName)
	{
		return UI_CONFIG_DIRECTORY + sceneName + "/";
	}
}

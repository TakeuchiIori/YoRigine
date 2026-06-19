#include "EventTriggerLoader.h"

#include "TriggerActionFactory.h"
#include "Debugger/Logger.h"

#include <filesystem>
#include <fstream>
#include <json.hpp>

namespace {

	Vector3 ReadVec3(const nlohmann::json& j, const Vector3& fallback) {
		if (!j.is_array() || j.size() < 3) return fallback;
		return Vector3{
			j[0].get<float>(),
			j[1].get<float>(),
			j[2].get<float>()
		};
	}

} // namespace

bool EventTriggerLoader::Load(
	const std::string& filePath,
	Camera* camera,
	std::function<void()> onAnyGateOpened,
	std::vector<std::unique_ptr<EventTrigger>>& outTriggers,
	std::vector<OpenGateAction*>& outOpenGateActions)
{
	std::filesystem::path path(filePath);
	if (!std::filesystem::exists(path)) {
		Logger("[EventTriggerLoader] ファイルが見つかりません (起動時は無視): " + filePath + "\n");
		return false;
	}

	std::ifstream file(filePath);
	if (!file.is_open()) {
		Logger("[EventTriggerLoader] ファイルを開けません: " + filePath + "\n");
		return false;
	}

	nlohmann::json json;
	try {
		file >> json;
	}
	catch (const std::exception& e) {
		Logger(std::string("[EventTriggerLoader] JSON パース失敗: ") + e.what() + "\n");
		return false;
	}

	if (!json.contains("triggers") || !json["triggers"].is_array()) {
		Logger("[EventTriggerLoader] triggers 配列が無いためスキップ\n");
		return true; // ファイル自体は読めたので true
	}

	int created = 0;
	for (const auto& t : json["triggers"]) {
		if (!t.contains("action")) {
			Logger("[EventTriggerLoader] action が無いトリガーをスキップ\n");
			continue;
		}

		auto action = TriggerActionFactory::Create(t["action"]);
		if (!action) {
			continue; // factory 側で原因ログ出力済み
		}

		// OpenGateAction の場合は dispatch リストに追加 + onGateOpened コールバック設定
		if (auto* openGate = dynamic_cast<OpenGateAction*>(action.get())) {
			openGate->SetOnGateOpened(onAnyGateOpened);
			outOpenGateActions.push_back(openGate);
		}

		auto trigger = std::make_unique<EventTrigger>();
		trigger->Initialize(camera);
		trigger->SetName(t.value("name", std::string{}));

		auto& wt = trigger->GetWT();
		wt.translate_ = ReadVec3(t.value("position", nlohmann::json::array()), {});
		wt.rotate_    = ReadVec3(t.value("rotation", nlohmann::json::array()), {});
		wt.scale_     = ReadVec3(t.value("scale",    nlohmann::json::array()), { 1.0f, 1.0f, 1.0f });
		wt.UpdateMatrix();

		trigger->SetAction(std::move(action));
		outTriggers.push_back(std::move(trigger));
		++created;
	}

	// 全 OpenGateAction について target を「閉」状態へ強制復元する。
	// これによりゲーム再起動時に Field.json が誤って "開いた pose" で保存されていても
	// 初期状態に戻る (closed pose 未捕捉のものは現在 target を「閉」として捕捉する)。
	for (auto* gate : outOpenGateActions) {
		if (gate) gate->RestoreClosed();
	}

	Logger("[EventTriggerLoader] " + std::to_string(created) + " 個のトリガーを生成: " + filePath + "\n");
	return true;
}

bool EventTriggerLoader::Save(
	const std::string& filePath,
	const std::vector<std::unique_ptr<EventTrigger>>& triggers)
{
	nlohmann::json root;
	root["triggers"] = nlohmann::json::array();

	for (const auto& trig : triggers) {
		if (!trig) continue;
		const auto& wt = trig->GetWT();

		nlohmann::json t;
		t["name"]     = trig->GetName();
		t["position"] = nlohmann::json::array({ wt.translate_.x, wt.translate_.y, wt.translate_.z });
		t["rotation"] = nlohmann::json::array({ wt.rotate_.x,    wt.rotate_.y,    wt.rotate_.z });
		t["scale"]    = nlohmann::json::array({ wt.scale_.x,     wt.scale_.y,     wt.scale_.z });

		if (auto* action = trig->GetAction()) {
			t["action"] = action->SerializeToJson();
		}
		root["triggers"].push_back(std::move(t));
	}

	try {
		std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
		std::ofstream file(filePath);
		if (!file.is_open()) {
			Logger("[EventTriggerLoader] 保存先を開けません: " + filePath + "\n");
			return false;
		}
		file << root.dump(2);
	}
	catch (const std::exception& e) {
		Logger(std::string("[EventTriggerLoader] 保存失敗: ") + e.what() + "\n");
		return false;
	}

	Logger("[EventTriggerLoader] 保存しました: " + filePath + "\n");
	return true;
}

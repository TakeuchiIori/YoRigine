#include "CameraEditor.h"
#include "CameraDirector.h"
#include "CameraFactory.h"
#include <imgui.h>
#include <fstream>

void CameraEditor::Initialize() {
	selectedCameraName_ = "";
}

void CameraEditor::LoadFileOrDefault(const std::string& filePath, const std::string& sceneType)
{
	std::ifstream file(filePath);
	if (file.is_open()) {
		// ✅ JSONが存在するなら普通に読む
		LoadFile(filePath);
	}
	else {
		// ✅ JSONがなければデフォルトを生成して保存
		InitializeDefaults(sceneType);
		SaveFile(filePath); // 次回からはJSONで管理できる
	}
}

void CameraEditor::Update() {
#ifdef USE_IMGUI
	// 新規カメラの追加
	if (ImGui::CollapsingHeader("カメラの追加", ImGuiTreeNodeFlags_DefaultOpen)) {
		static int selectedTypeIdx = 0;
		auto types = CameraFactory::GetTypeList(); // ["DebugCamera", "FollowCamera"] 等

		// 型の選択コンボボックス
		if (ImGui::BeginCombo("種類", types[selectedTypeIdx].c_str())) {
			for (int i = 0; i < types.size(); i++) {
				if (ImGui::Selectable(types[i].c_str(), selectedTypeIdx == i)) selectedTypeIdx = i;
			}
			ImGui::EndCombo();
		}

		ImGui::InputText("名前", newCameraName_, IM_ARRAYSIZE(newCameraName_));

		if (ImGui::Button("新規作成・登録")) {
			auto newCam = CameraFactory::Create(types[selectedTypeIdx]);
			if (newCam) {
				newCam->Initialize();
				// ここで名前をセットしてDirectorに登録！
				CameraDirector::GetInstance()->AddCamera(newCameraName_, newCam);
				selectedCameraName_ = newCameraName_;
			}
		}
	}

	ImGui::Separator();

	// カメラリスト
	ImGui::Text("登録済みカメラ一覧");
	auto& cameras = CameraDirector::GetInstance()->GetAllCameras();

	for (auto& [name, cam] : cameras) {
		bool isSelected = (selectedCameraName_ == name);
		if (ImGui::Selectable(name.c_str(), isSelected)) {
			selectedCameraName_ = name;
		}
	}

	ImGui::Separator();

	// 選択中カメラの詳細設定
	if (!selectedCameraName_.empty()) {
		auto cam = CameraDirector::GetInstance()->GetCamera(selectedCameraName_);
		if (cam) {
			ImGui::Text("編集中のカメラ: %s", selectedCameraName_.c_str());

			// 共通パラメータ
			int priority = cam->GetPriority();
			if (ImGui::DragInt("優先度 (Priority)", &priority, 1, 0, 100)) {
				cam->SetPriority(priority);
			}
			cam->DrawDebugGui();
		}
	}

	ImGui::Separator();

	// 保存・読み込み
	if (ImGui::Button("すべての設定を保存")) {
		SaveFile(filePath_);
	}
	ImGui::SameLine();
	if (ImGui::Button("設定を読み込み")) {
		LoadFile(filePath_);
	}
#endif // USE_IMGUI
}

void CameraEditor::SaveFile(const std::string& filePath) {
	nlohmann::json root;
	auto& cameras = CameraDirector::GetInstance()->GetAllCameras();

	for (auto& [name, cam] : cameras) {
		nlohmann::json camJson;
		// 型情報を保存するのが最重要！ (Factoryが復元に使う)
		camJson["type"] = CameraFactory::GetTypeName(cam);
		cam->Save(camJson);
		root["cameras"].push_back(camJson);
	}

	std::ofstream file(filePath);
	if (file.is_open()) {
		file << root.dump(4);
	}
}

void CameraEditor::LoadFile(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) return;

	nlohmann::json root;
	file >> root;

	// 現在のカメラを一度クリア（任意）
	CameraDirector::GetInstance()->Initialize();

	for (auto& item : root["cameras"]) {
		std::string type = item["type"];
		auto cam = CameraFactory::Create(type);
		if (cam) {
			cam->Initialize();
			cam->Load(item);
			CameraDirector::GetInstance()->AddCamera(cam->GetName(), cam);
		}
	}
	CameraDirector::GetInstance()->SnapToActiveCamera();
}

void CameraEditor::InitializeDefaults(const std::string& sceneType)
{
	auto director = CameraDirector::GetInstance();

	// デバッグカメラは共通で必ず追加
	auto debug = CameraFactory::Create("Debug");
	debug->Initialize();
	director->AddCamera("MainDebug", debug);

	if (sceneType == "Game") {
		auto follow = CameraFactory::Create("Follow");
		follow->Initialize();
		director->AddCamera("PlayerFollow", follow);

	}
	else if (sceneType == "Clear") {
		auto clear = CameraFactory::Create("Clear");
		clear->Initialize();
		director->AddCamera("ClearCamera", clear);
	}
	else if (sceneType == "Title") {
		auto title = CameraFactory::Create("Title");
		title->Initialize();
		director->AddCamera("TitleCamera", title);
	}
}

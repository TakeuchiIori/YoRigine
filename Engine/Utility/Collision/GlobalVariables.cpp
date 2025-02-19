#include "GlobalVariables.h"
// C++
#include <fstream>
#include "Windows.h"

#ifdef _DEBUG
#include <imgui.h>
#endif 


using namespace std;
void GlobalVariables::Update() {
#ifdef _DEBUG

	if (!ImGui::Begin("Global Variables", nullptr, ImGuiWindowFlags_MenuBar)) {
		ImGui::End();
		return;

	}

	if (ImGui::BeginMenuBar()) {
		// 各グループについて
		for (map<string, Group>::iterator itGroup = datas_.begin(); itGroup != datas_.end(); ++itGroup) {
			// グループ名を取得
			const string& groupName = itGroup->first;
			// グループの参照を取得
			Group& group = itGroup->second;

			if (ImGui::BeginMenu(groupName.c_str())) {
				// 各項目について
				for (map<string, Item>::iterator itItem = group.items.begin(); itItem != group.items.end(); ++itItem) {
					// 項目名を取得
					const string& itemName = itItem->first;
					// 項目の参照を取得
					Item& item = itItem->second;

					// int32_t型の値を保持していれば
					if (holds_alternative<int32_t>(item.value)) {
						int32_t* ptr = get_if<int32_t>(&item.value);
						ImGui::SliderInt(itemName.c_str(), ptr, 0, 100);
					} // float型の値を保持していれば
					else if (holds_alternative<float>(item.value)) {
						float* ptr = get_if<float>(&item.value);
						ImGui::SliderFloat(itemName.c_str(), ptr, 0, 100);
					} // Vector3型の値を保持していれば
					else if (holds_alternative<Vector3>(item.value)) {
						Vector3* ptr = get_if<Vector3>(&item.value);
						ImGui::SliderFloat3(itemName.c_str(), reinterpret_cast<float*>(ptr), -10.0f, 10.0f);
					} // bool型の値を保持していれば
					else if (std::holds_alternative<bool>(item.value)) {
						bool* ptr = std::get_if<bool>(&item.value);
						ImGui::Checkbox(itemName.c_str(), ptr);
					}
				}
				// 改行
				ImGui::Text("\n");

				if (ImGui::Button("Save")) {
					SaveFile(groupName);
					string message = format("{}.json saved.", groupName);
					MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
				}

				ImGui::EndMenu();
			}
		}
		ImGui::EndMenuBar();
	}

	ImGui::End();

#endif // _DEBUG
}

GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables instance;

	return &instance;
}

void GlobalVariables::CreateGroup(const string& groupName) {
	// 指定名のオブジェクトがなければ追加する
	datas_[groupName];
}

void GlobalVariables::SaveFile(const string& groupName) {
	// グループを検索
	map<string, Group>::iterator itGroup = datas_.find(groupName);

	// 未登録チェック
	assert(itGroup != datas_.end());

	json root;
	root = json::object();

	// jsonオブジェクト登録
	root[groupName] = json::object();

	// 各項目について
	for (map<string, Item>::iterator itItem = itGroup->second.items.begin();
		itItem != itGroup->second.items.end(); ++itItem) {

		// 項目名を取得
		const string& itemName = itItem->first;
		// 項目の参照を取得
		Item& item = itItem->second;

		// int32_t型の値を保持していれば
		if (holds_alternative<int32_t>(item.value)) {
			// int32_t型の値を登録
			root[groupName][itemName] = get<int32_t>(item.value);
		}
		// float型の値を保持していれば
		else if (holds_alternative<float>(item.value)) {
			// float型の値を登録
			root[groupName][itemName] = get<float>(item.value);
		}
		// Vector3型の値を保持していれば
		else if (holds_alternative<Vector3>(item.value)) {
			// float型のjson配列を登録
			Vector3 value = get<Vector3>(item.value);
			root[groupName][itemName] = json::array({ value.x, value.y, value.z });
		}// bool型の値を保持していれば
		else if (std::holds_alternative<bool>(item.value)) {
			// bool型の値を登録
			root[groupName][itemName] = std::get<bool>(item.value);
		}
	}
	// ディレクトリが無ければ作成する
	filesystem::path dir(kDirectoryPath);
	if (!filesystem::exists(kDirectoryPath)) {
		filesystem::create_directory(kDirectoryPath);
	}
	// 書き込むJSONファイルのフルパスを合成する
	string filePath = kDirectoryPath + groupName + ".json";
	// 書き込み用ファイナルストリーム
	ofstream ofs;
	// ファイルを書き込み用に開く
	ofs.open(filePath);
	// ファイルオープン失敗？
	if (ofs.fail()) {
		string message = "Failed open data file for write";
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}
	// ファイルにjson文字列を書き込む（インデックス幅4）
	ofs << setw(4) << root << endl;
	// ファイルを閉じる
	ofs.close();
}

void GlobalVariables::LoadFiles() {
	// ローカル変数の保存先ファイルパス
	const string directoryPath = kDirectoryPath;

	// ディレクトリがなければスキップする
	if (!std::filesystem::exists(directoryPath)) {
		return;
	}

	filesystem::directory_iterator dir_it(directoryPath);
	for (const filesystem::directory_entry& entry : dir_it) {
		// ファイルパスを取得
		const filesystem::path& filePath = entry.path();

		// ファイル拡張子を取得
		string extension = filePath.extension().string();
		// .jsonファイル以外はスキップ
		if (extension.compare(".json") != 0) {
			continue;
		}
		// ファイル読み込み
		LoadFile(filePath.stem().string());
	}
}


void GlobalVariables::LoadFile(const std::string& groupName) {
	// 読み込むJSONファイルのフルパスを合成する
	string filePath = kDirectoryPath + groupName + ".json";
	// 読み込む用ファイルストリーム
	ifstream ifs;
	// ファイルを読み込むように開く
	ifs.open(filePath);
	// ファイルオープン失敗
	if (ifs.fail()) {
		string message = "Failed open data file for write";
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}

	json root;

	// json文字列からjsonのデータ構造に展開
	ifs >> root;
	// ファイルを閉じる
	ifs.close();

	// グループ名を検索
	json::iterator itGroup = root.find(groupName);

	// 未登録チェック
	assert(itGroup != root.end());

	// 各アイテムについて
	for (json::iterator itItem = itGroup->begin(); itItem != itGroup->end(); ++itItem) {
		// アイテム名そ取得
		const string& itemName = itItem.key();
		// int32_t型の値を保持していれば
		if (itItem->is_number_integer()) {
			// int型の値を登録
			int32_t value = itItem->get<int32_t>();
			SetValue(groupName, itemName, value);
		} // float型の値を保持していれば
		else if (itItem->is_number_float()) {
			// int型の値を登録
			double value = itItem->get<double>();
			SetValue(groupName, itemName, static_cast<float>(value));
		} // Vector3型の値を保持していれば
		else if (itItem->is_array() && itItem->size() == 3) {
			// float型のjson配列登録
			Vector3 value = { itItem->at(0), itItem->at(1), itItem->at(2) };
			SetValue(groupName, itemName, value);
		} // bool型の値を保持していれば
		else if (itItem->is_boolean()) {
			// bool型の値を登録
			bool value = itItem->get<bool>();
			SetValue(groupName, itemName, value);
		}
	}

}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, int32_t value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 項目が未登録なら
	if (group.items.find(key) == group.items.end()) {
		SetValue(groupName, key, value);
	}
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, float value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 項目が未登録なら
	if (group.items.find(key) == group.items.end()) {
		SetValue(groupName, key, value);
	}
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector3& value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 項目が未登録なら
	if (group.items.find(key) == group.items.end()) {
		SetValue(groupName, key, value);
	}
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Quaternion& value)
{
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 項目が未登録なら
	if (group.items.find(key) == group.items.end()) {
		SetValue(groupName, key, value);
	}
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const bool& value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 項目が未登録なら
	if (group.items.find(key) == group.items.end()) {
		SetValue(groupName, key, value);
	}
}


void GlobalVariables::SetValue(const string& groupName, const string& key, int32_t value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 新しい項目のデータを設定
	Item newItem{};
	newItem.value = value;
	// 設定した項目をmapに追加
	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const string& groupName, const string& key, float value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 新しい項目のデータを設定
	Item newItem{};
	newItem.value = value;
	// 設定した項目をmapに追加
	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const string& groupName, const string& key, const Vector3 value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 新しい項目のデータを設定
	Item newItem{};
	newItem.value = value;
	// 設定した項目をmapに追加
	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Quaternion value)
{
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 新しい項目のデータを設定
	Item newItem{};
	newItem.value = value;
	// 設定した項目をmapに追加
	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const string& groupName, const string& key, const bool& value) {
	// グループの参照を取得
	Group& group = datas_[groupName];
	// 新しい項目のデータを設定
	Item newItem{};
	newItem.value = value;
	// 設定した項目をmapに追加
	group.items[key] = newItem;
}

int32_t GlobalVariables::GetIntValue(const std::string& groupName, const std::string& key) const {
	// 指定グループが存在するか確認
	assert(datas_.find(groupName) != datas_.end());
	// グループの参照を取得
	const Group& group = datas_.at(groupName);
	// 指定グループに指定のキーが存在するか確認
	assert(group.items.find(key) != group.items.end());
	// アイテムの参照を取得
	const Item& item = group.items.at(key);
	// int32_t型の値を保持しているか確認
	assert(std::holds_alternative<int32_t>(item.value));
	// 指定グループから指定のキーの値を取得
	return std::get<int32_t>(item.value);
}

float GlobalVariables::GetFloatValue(const std::string& groupName, const std::string& key) const {
	// 指定グループが存在するか確認
	assert(datas_.find(groupName) != datas_.end());
	// グループの参照を取得
	const Group& group = datas_.at(groupName);
	// 指定グループに指定のキーが存在するか確認
	assert(group.items.find(key) != group.items.end());
	// アイテムの参照を取得
	const Item& item = group.items.at(key);
	// float型の値を保持しているか確認
	assert(std::holds_alternative<float>(item.value));
	// 指定グループから指定のキーの値を取得
	return std::get<float>(item.value);
}

Vector3 GlobalVariables::GetVector3Value(const std::string& groupName, const std::string& key) const {
	// 指定グループが存在するか確認
	assert(datas_.find(groupName) != datas_.end());
	// グループの参照を取得
	const Group& group = datas_.at(groupName);
	// 指定グループに指定のキーが存在するか確認
	assert(group.items.find(key) != group.items.end());
	// アイテムの参照を取得
	const Item& item = group.items.at(key);
	// Vector3型の値を保持しているか確認
	assert(std::holds_alternative<Vector3>(item.value));
	// 指定グループから指定のキーの値を取得
	return std::get<Vector3>(item.value);
}

bool GlobalVariables::GetBoolValue(const std::string& groupName, const std::string& key) const {
	// 指定グループが存在するか確認
	assert(datas_.find(groupName) != datas_.end());
	// グループの参照を取得
	const Group& group = datas_.at(groupName);
	// 指定グループに指定のキーが存在するか確認
	assert(group.items.find(key) != group.items.end());
	// アイテムの参照を取得
	const Item& item = group.items.at(key);
	// bool型の値を保持しているか確認
	assert(std::holds_alternative<bool>(item.value));
	// 指定グループから指定のキーの値を取得
	return std::get<bool>(item.value);
}

Quaternion GlobalVariables::GetQuaternionValue(const std::string& groupName, const std::string& key) const
{
	// 指定グループが存在するか確認
	assert(datas_.find(groupName) != datas_.end());
	// グループの参照を取得
	const Group& group = datas_.at(groupName);
	// 指定グループに指定のキーが存在するか確認
	assert(group.items.find(key) != group.items.end());
	// アイテムの参照を取得
	const Item& item = group.items.at(key);
	// bool型の値を保持しているか確認
	assert(std::holds_alternative<Quaternion>(item.value));
	// 指定グループから指定のキーの値を取得
	return std::get<Quaternion>(item.value);
}

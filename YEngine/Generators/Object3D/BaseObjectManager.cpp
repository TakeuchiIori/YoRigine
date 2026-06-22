#include "BaseObjectManager.h"

// Engine
#include "Debugger/Logger.h"

// C++
#include <algorithm>

#ifdef USE_IMGUI
#include <Editor/Editor.h>
#include "imgui.h"
#endif

BaseObjectManager* BaseObjectManager::instance_ = nullptr;

// ============================================================
// シングルトンインスタンス取得
// ============================================================
BaseObjectManager* BaseObjectManager::GetInstance() {
	if (!instance_) {
		instance_ = new BaseObjectManager;
	}
	return instance_;
}

// ============================================================
// マネージャ初期化
// ============================================================
void BaseObjectManager::Initialize() {
	entries_.clear();
	selectedIndex_ = -1;

#ifdef USE_IMGUI
	//------------------------------------------------------------
	// インスペクタパネルを Editor に登録 (Debug のみ・一度きり)
	//------------------------------------------------------------
	if (!inspectorRegistered_) {
		Editor::GetInstance()->RegisterGameUI(
			"オブジェクト一覧",
			[this]() { this->DrawInspector(); });
		inspectorRegistered_ = true;
	}
#endif
}

// ============================================================
// 全オブジェクト破棄して終了
// ============================================================
void BaseObjectManager::Finalize() {
	ClearAll();
}

// ============================================================
// 生成済みオブジェクトを駆動対象に登録 (所有は移さない)
// ============================================================
void BaseObjectManager::Register(BaseObject* obj, const std::string& name) {
	if (!obj) return;

	//------------------------------------------------------------
	// 二重登録チェック
	//------------------------------------------------------------
	for (const auto& entry : entries_) {
		if (entry.ptr == obj) {
			Logger("[BaseObjectManager] Register : 既に登録済みのオブジェクトです\n");
			return;
		}
	}

	//------------------------------------------------------------
	// 名前を確定 (引数優先、空ならオブジェクト側の既存名を使う)
	//------------------------------------------------------------
	if (!name.empty()) {
		obj->SetName(name);
	}

	Entry entry;
	entry.ptr = obj;
	entry.owned = nullptr;          // 外部所有なので実体は持たない
	entry.name = obj->GetName();
	entries_.push_back(std::move(entry));
}

// ============================================================
// 駆動対象から外す
// ============================================================
void BaseObjectManager::Unregister(BaseObject* obj) {
	if (!obj) return;

	auto it = std::find_if(entries_.begin(), entries_.end(),
		[obj](const Entry& e) { return e.ptr == obj; });

	if (it != entries_.end()) {
		// owned が非 null なら unique_ptr のデストラクタが実体を破棄する。
		entries_.erase(it);
		selectedIndex_ = -1;
	}
}

// ============================================================
// 遅延破棄の予約
// ============================================================
void BaseObjectManager::Destroy(BaseObject* obj) {
	if (!obj) return;

	for (auto& entry : entries_) {
		if (entry.ptr == obj) {
			entry.pendingDestroy = true;
			return;
		}
	}
}

// ============================================================
// 全オブジェクトをクリア
// ============================================================
void BaseObjectManager::ClearAll() {
	// owned を持つエントリは erase 時に自動破棄される。
	entries_.clear();
	selectedIndex_ = -1;
}

// ============================================================
// 一括更新
// ============================================================
void BaseObjectManager::UpdateAll() {
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (!entry.ptr->IsActive()) continue;
		entry.ptr->Update();
	}

	//------------------------------------------------------------
	// フレーム末: 破棄予約をまとめて反映する
	//------------------------------------------------------------
	ProcessPendingDestroy();
}

// ============================================================
// 一括描画
// ============================================================
void BaseObjectManager::DrawAll() {
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (!entry.ptr->IsActive()) continue;
		entry.ptr->Draw();
	}
}

// ============================================================
// 一括アニメーション描画
// ============================================================
void BaseObjectManager::DrawAnimationAll() {
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (!entry.ptr->IsActive()) continue;
		entry.ptr->DrawAnimation();
	}
}

// ============================================================
// 一括コリジョン描画
// ============================================================
void BaseObjectManager::DrawCollisionAll() {
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (!entry.ptr->IsActive()) continue;
		entry.ptr->DrawCollision();
	}
}

// ============================================================
// 一括影描画
// ============================================================
void BaseObjectManager::DrawShadowAll() {
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (!entry.ptr->IsActive()) continue;
		entry.ptr->DrawShadow();
	}
}

// ============================================================
// 名前で検索
// ============================================================
BaseObject* BaseObjectManager::FindByName(const std::string& name) {
	if (name.empty()) return nullptr;

	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (entry.ptr->GetName() == name) {
			return entry.ptr;
		}
	}
	return nullptr;
}

// ============================================================
// 破棄予約の実反映
// ============================================================
void BaseObjectManager::ProcessPendingDestroy() {
	if (entries_.empty()) return;

	const bool removed = std::any_of(entries_.begin(), entries_.end(),
		[](const Entry& e) { return e.pendingDestroy; });
	if (!removed) return;

	//------------------------------------------------------------
	// pendingDestroy を立てたエントリを除去 (owned は自動破棄)
	//------------------------------------------------------------
	entries_.erase(
		std::remove_if(entries_.begin(), entries_.end(),
			[](const Entry& e) { return e.pendingDestroy; }),
		entries_.end());

	// 選択中の行がずれる可能性があるためリセットする。
	selectedIndex_ = -1;
}

// ============================================================
// インスペクタ描画 (Debug のみ)
// ============================================================
void BaseObjectManager::DrawInspector() {
#ifdef USE_IMGUI
	//------------------------------------------------------------
	// 左: 登録オブジェクト一覧
	//------------------------------------------------------------
	ImGui::Text("登録オブジェクト数: %d", GetObjectCount());
	ImGui::Separator();

	if (ImGui::BeginChild("ObjectList", ImVec2(220.0f, 0.0f), true)) {
		for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
			Entry& entry = entries_[i];
			if (!entry.ptr) continue;

			ImGui::PushID(i);

			// アクティブトグル
			bool active = entry.ptr->IsActive();
			if (ImGui::Checkbox("##active", &active)) {
				entry.ptr->SetActive(active);
			}
			ImGui::SameLine();

			// 名前 (空なら型名でフォールバック表示)
			const std::string& name = entry.ptr->GetName();
			const char* label = !name.empty() ? name.c_str() : typeid(*entry.ptr).name();
			if (ImGui::Selectable(label, selectedIndex_ == i)) {
				selectedIndex_ = i;
			}

			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	//------------------------------------------------------------
	// 右: 選択オブジェクトの詳細
	//------------------------------------------------------------
	if (ImGui::BeginChild("ObjectDetail", ImVec2(0.0f, 0.0f), true)) {
		if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(entries_.size())
			&& entries_[selectedIndex_].ptr) {

			BaseObject* obj = entries_[selectedIndex_].ptr;

			ImGui::Text("型: %s", typeid(*obj).name());
			ImGui::Text("所有: %s",
				entries_[selectedIndex_].owned ? "Manager (Add)" : "外部 (Register)");
			ImGui::Separator();

			// SRT (BaseObject の公開アクセサ経由で共通編集)
			Vector3 translate = obj->GetTranslate();
			if (ImGui::DragFloat3("Position", &translate.x, 0.01f)) {
				obj->SetTranslate(translate);
			}
			Vector3 rotate = obj->GetRotae();
			if (ImGui::DragFloat3("Rotation", &rotate.x, 0.01f)) {
				obj->SetRotae(rotate);
			}
			Vector3 scale = obj->GetScale();
			if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
				obj->SetScale(scale);
			}

			ImGui::Separator();

			// オブジェクト固有の詳細 (オーバーライドしていれば描画される)
			obj->DrawInspector();
		} else {
			ImGui::TextDisabled("オブジェクトを選択してください");
		}
	}
	ImGui::EndChild();
#endif
}

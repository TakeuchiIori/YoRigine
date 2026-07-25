#include "BaseObjectManager.h"

// Engine
#include "Debugger/Logger.h"
#include "Drawer/InstancedObject3d.h"

// C++
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include <Editor/Editor.h>
#endif

// ============================================================
// シングルトンインスタンス取得
// ============================================================
BaseObjectManager *BaseObjectManager::GetInstance() {
  static BaseObjectManager instance;
  return &instance;
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
    assert(editor_ &&
           "BaseObjectManager::Initialize : SetEditor() を先に呼ぶこと");
    editor_->RegisterGameUI(
        "オブジェクト一覧", [this]() { this->DrawInspector(); }, "AllScene",
        "シーン", true);
    inspectorRegistered_ = true;
  }
#endif
}

// ============================================================
// 全オブジェクト破棄して終了
// ============================================================
void BaseObjectManager::Finalize() {
  ClearAll();

  // instancedObject3d_ / editor_ はアプリ生存期間中ずっと生きるシングルトンを
  // MyGame::Initialize()
  // で一度だけ借用しているだけなので、ここで手放してはいけない。 手放すと 2
  // 回目以降のシーン入場で null のままになり、DrawAllInstanced() の assert
  // が発火して停止する。シングルトンなのでダングリングは起きない。
}

// ============================================================
// 生成済みオブジェクトを駆動対象に登録 (所有は移さない)
// ============================================================
void BaseObjectManager::Register(YoRigine::BaseObject *obj, const std::string &name) {
  if (!obj)
    return;

  //------------------------------------------------------------
  // 二重登録チェック
  //------------------------------------------------------------
  for (const auto &entry : entries_) {
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
  entry.owned = nullptr; // 外部所有なので実体は持たない
  entry.name = obj->GetName();
  entries_.push_back(std::move(entry));
}

// ============================================================
// 駆動対象から外す
// ============================================================
void BaseObjectManager::Unregister(YoRigine::BaseObject *obj) {
  if (!obj)
    return;

  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [obj](const Entry &e) { return e.ptr == obj; });

  if (it != entries_.end()) {
    // owned が非 null なら unique_ptr のデストラクタが実体を破棄する。
    entries_.erase(it);
    selectedIndex_ = -1;
  }
}

// ============================================================
// 遅延破棄の予約
// ============================================================
void BaseObjectManager::Destroy(YoRigine::BaseObject *obj) {
  if (!obj)
    return;

  for (auto &entry : entries_) {
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
  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;
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
void BaseObjectManager::DrawAll() { DrawAllInstanced(); }

// ============================================================
// 一括アニメーション描画
// ============================================================
void BaseObjectManager::DrawAnimationAll() {
  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;
    entry.ptr->DrawAnimation();
  }
}

// ============================================================
// 一括コリジョン描画
// ============================================================
void BaseObjectManager::DrawCollisionAll() {
  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;
    entry.ptr->DrawCollision();
  }
}

// ============================================================
// 一括影描画
// ============================================================
void BaseObjectManager::DrawShadowAll() {
  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;
    entry.ptr->DrawShadow();
  }
}

// ============================================================
// 一括描画（インスタンシング対応・カラーパス）
// ============================================================
void BaseObjectManager::DrawAllInstanced() {
  if (!instancedObject3d_ || !camera_) {
    return;
  }
  assert(instancedObject3d_ &&
         "BaseObjectManager : SetInstancedObject3d() を先に呼ぶこと");
  auto *inst = instancedObject3d_;
  inst->Begin(camera_);

  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;

    if (entry.ptr->IsInstanceable()) {
      // 非アニメ: インスタンスバッファに積む（材質は Object3d から取り出す）
      inst->Submit(*entry.ptr->GetObject3d(), entry.ptr->GetWT());
    } else {
      // アニメ / 特殊描画: 従来どおり個別 Draw（本体が DrawAnimation
      // にある型もあるため両方）
      entry.ptr->Draw();
      entry.ptr->DrawAnimation();
    }
  }

  inst->DrawAll(camera_);
}

// ============================================================
// 一括影描画（インスタンシング対応・影パス）
// ============================================================
void BaseObjectManager::DrawShadowAllInstanced() {
  if (!instancedObject3d_ || !camera_) {
    return;
  }
  assert(instancedObject3d_ &&
         "BaseObjectManager : SetInstancedObject3d() を先に呼ぶこと");
  auto *inst = instancedObject3d_;
  inst->Begin(); // 影は WVP を使わないのでカメラ不要

  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
    if (!entry.ptr->IsActive())
      continue;

    if (entry.ptr->IsInstanceable()) {
      inst->Submit(*entry.ptr->GetObject3d(), entry.ptr->GetWT());
    } else {
      entry.ptr->DrawShadow();
    }
  }

  inst->DrawShadow();
}

// ============================================================
// 名前で検索
// ============================================================
YoRigine::BaseObject *BaseObjectManager::FindByName(const std::string &name) {
  if (name.empty())
    return nullptr;

  for (auto &entry : entries_) {
    if (!entry.ptr || entry.pendingDestroy)
      continue;
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
  if (entries_.empty())
    return;

  const bool removed =
      std::any_of(entries_.begin(), entries_.end(),
                  [](const Entry &e) { return e.pendingDestroy; });
  if (!removed)
    return;

  //------------------------------------------------------------
  // pendingDestroy を立てたエントリを除去 (owned は自動破棄)
  //------------------------------------------------------------
  entries_.erase(
      std::remove_if(entries_.begin(), entries_.end(),
                     [](const Entry &e) { return e.pendingDestroy; }),
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
      Entry &entry = entries_[i];
      if (!entry.ptr)
        continue;

      ImGui::PushID(i);

      // アクティブトグル
      bool active = entry.ptr->IsActive();
      if (ImGui::Checkbox("##active", &active)) {
        entry.ptr->SetActive(active);
      }
      ImGui::SameLine();

      // 名前 (空なら型名でフォールバック表示)
      const std::string &name = entry.ptr->GetName();
      const char *label =
          !name.empty() ? name.c_str() : typeid(*entry.ptr).name();
      if (ImGui::Selectable(label, selectedIndex_ == i)) {
        selectedIndex_ = i;
      }

      ImGui::PopID();
    }
  }
  ImGui::EndChild();
#endif
}

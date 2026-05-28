#ifdef USE_IMGUI

#include "YEmitterGroupEditor.h"
#include "YParticleManager.h"
#include <Editor/Icon/EditorIcon.h>
#include <Editor/Editor.h>
#include <algorithm>
#include <cmath>
#include "imgui.h"


static void Splitter(float thickness, float* size1, float min_size1, float min_size2, float height = -1.0f) {
    ImVec2 backup_pos = ImGui::GetCursorPos();
    if (height < 0) height = ImGui::GetContentRegionAvail().y;
    ImGui::Button("##vsplitter", ImVec2(thickness, height));
    if (ImGui::IsItemActive()) {
        float delta = ImGui::GetIO().MouseDelta.x;
        if (delta != 0.0f) {
            *size1 += delta;
            if (*size1 < min_size1) *size1 = min_size1;
            if (*size1 > ImGui::GetContentRegionAvail().x - min_size2 - thickness)
                *size1 = ImGui::GetContentRegionAvail().x - min_size2 - thickness;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SetCursorPos(backup_pos);
}

YEmitterGroupEditor::YEmitterGroupEditor()
    : saveBrowser_(emitterDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
    , loadBrowser_(emitterDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List) {
    gizmoCtrl_.Initialize();
    saveBrowser_.SetOnFileSelected([this](const std::string& path) {
        bool ok = YEmitterGroupManager::GetInstance().SaveAllToFile(path);
        showSavePopup_ = false;
        SetNotification(ok, ok ? "保存完了: " + path : "保存失敗: " + path);
        });
    loadBrowser_.SetOnFileSelected([this](const std::string& path) {
        bool ok = YEmitterGroupManager::GetInstance().LoadAllFromFile(path);
        showLoadPopup_ = false;
        SetNotification(ok, ok ? "読み込み完了: " + path : "読み込み失敗: " + path);
        });
}

void YEmitterGroupEditor::SetNotification(bool success, const std::string& message) {
    saveNotifySuccess_ = success;
    saveNotifyMessage_ = message;
    saveNotifyTimer_ = 3.0f;
}

void YEmitterGroupEditor::ShowSaveNotification() {
    if (saveNotifyTimer_ <= 0.0f) return;
    saveNotifyTimer_ -= ImGui::GetIO().DeltaTime;
    float alpha = std::min(1.0f, saveNotifyTimer_ / 0.5f);
    ImVec4 col = saveNotifySuccess_
        ? ImVec4(0.2f, 1.0f, 0.3f, alpha)
        : ImVec4(1.0f, 0.3f, 0.2f, alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::Text("%s %s",
        saveNotifySuccess_ ? Icon::CheckCircle : Icon::TimesCircle,
        saveNotifyMessage_.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void YEmitterGroupEditor::DrawGizmo() {
    if (!camera_ || gizmoTargets_.empty()) return;
    gizmoCtrl_.Draw(camera_, gizmoTargets_,
        Editor::GetInstance()->GetGameViewPos(),
        Editor::GetInstance()->GetGameViewSize());
}

void YEmitterGroupEditor::SelectGroup(const std::string& groupName) {
    selectedGroupName_ = groupName;
    selectedEmitterIndices_.clear();
    selectedEmitterIndex_ = -1;
    selectionMode_ = SelectionMode::Group;
    RebuildGizmoTargets();
}

void YEmitterGroupEditor::SelectEmitter(const std::string& groupName, int index, bool multiSelect) {
    if (groupName != selectedGroupName_) {
        selectedGroupName_ = groupName;
        selectedEmitterIndices_.clear();
    }
    selectionMode_ = SelectionMode::Emitter;
    if (multiSelect) {
        if (selectedEmitterIndices_.count(index)) selectedEmitterIndices_.erase(index);
        else selectedEmitterIndices_.insert(index);
    }
    else {
        selectedEmitterIndices_.clear();
        selectedEmitterIndices_.insert(index);
    }
    selectedEmitterIndex_ = index;
    RebuildGizmoTargets();
}

void YEmitterGroupEditor::RebuildGizmoTargets() {
    groupGizmable_.reset();
    emitterGizmables_.clear();
    gizmoTargets_.clear();
    auto& mgr = YEmitterGroupManager::GetInstance();
    if (selectionMode_ == SelectionMode::Group) {
        auto* group = mgr.GetGroup(selectedGroupName_);
        if (!group) return;
        groupGizmable_ = std::make_unique<YEmitterGroupGizmable>(group);
        gizmoTargets_.push_back(groupGizmable_.get());
    }
    else if (selectionMode_ == SelectionMode::Emitter) {
        auto* group = mgr.GetGroup(selectedGroupName_);
        if (!group) return;
        for (int idx : selectedEmitterIndices_) {
            auto* emitter = group->GetEmitter(static_cast<size_t>(idx));
            if (!emitter) continue;
            auto gizmable = std::make_unique<YEmitterGizmable>(
                emitter, selectedGroupName_ + "[" + std::to_string(idx) + "]");
            gizmoTargets_.push_back(gizmable.get());
            emitterGizmables_.push_back(std::move(gizmable));
        }
    }
}

void YEmitterGroupEditor::ShowEditor() {
    ShowSaveNotification();
    ShowGizmoToolbar();
    ImGui::Separator();
    float availHeight = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##GroupList", ImVec2(groupListWidth_, 0), true);
    ShowGroupList();
    ImGui::EndChild();
    ImGui::SameLine();
    Splitter(4.0f, &groupListWidth_, 120.0f, 200.0f, availHeight);
    ImGui::SameLine();
    ImGui::BeginChild("##GroupDetail", ImVec2(0, 0), true);
    ShowGroupDetail();
    ImGui::EndChild();
}

void YEmitterGroupEditor::ShowGizmoToolbar() { gizmoCtrl_.DrawSettings(); }

void YEmitterGroupEditor::ShowGroupList() {
    ImGui::Text("エミッターグループ");
    ImGui::Separator();
    auto& mgr = YEmitterGroupManager::GetInstance();
    auto  names = mgr.GetAllGroupNames();
    for (const auto& name : names) {
        auto* group = mgr.GetGroup(name);
        bool  active = group ? group->IsActive() : false;
        if (!active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        bool isSelected = (name == selectedGroupName_);
        if (ImGui::Selectable(name.c_str(), isSelected)) SelectGroup(name);
        if (!active) ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem(name.c_str())) {
            if (ImGui::MenuItem("グループギズモを選択")) SelectGroup(name);
            ImGui::Separator();
            if (ImGui::MenuItem("グループを削除")) {
                mgr.RemoveGroup(name);
                if (selectedGroupName_ == name) {
                    selectedGroupName_.clear();
                    selectionMode_ = SelectionMode::None;
                    RebuildGizmoTargets();
                }
                ImGui::EndPopup();
                break;
            }
            if (group) {
                if (ImGui::MenuItem(active ? "無効化" : "有効化"))
                    group->SetActive(!active);
            }
            ImGui::EndPopup();
        }
    }
    ImGui::Separator();
    ShowCreateGroupUI();
    ImGui::Separator();
    ShowFileButtons();
}

void YEmitterGroupEditor::ShowCreateGroupUI() {
    ImGui::Text("新規グループ");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##NewGroupName", newGroupNameBuf_, sizeof(newGroupNameBuf_));
    if (ImGui::Button("グループを作成", ImVec2(-1, 0))) {
        if (newGroupNameBuf_[0] != '\0') {
            YEmitterGroupManager::GetInstance().CreateGroup(newGroupNameBuf_);
            SelectGroup(newGroupNameBuf_);
            newGroupNameBuf_[0] = '\0';
        }
    }
}

void YEmitterGroupEditor::ShowGroupDetail() {
    if (selectedGroupName_.empty()) {
        ImGui::TextDisabled("← グループを選択または作成してください");
        return;
    }
    auto& mgr = YEmitterGroupManager::GetInstance();
    auto* group = mgr.GetGroup(selectedGroupName_);
    if (!group) {
        ImGui::TextColored({ 1, 0, 0, 1 }, "グループが見つかりません: %s", selectedGroupName_.c_str());
        return;
    }

    // ── グループ名・有効フラグ・位置 ─────────────────────────
    char nameBuf[128];
    strncpy_s(nameBuf, group->GetName().c_str(), sizeof(nameBuf));
    ImGui::Text("グループ: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##GName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newName = nameBuf;
        if (!newName.empty() && newName != selectedGroupName_) {
            auto* newGroup = mgr.CreateGroup(newName);
            newGroup->LoadFromJson(group->SaveToJson());
            newGroup->SetName(newName);
            mgr.RemoveGroup(selectedGroupName_);
            selectedGroupName_ = newName;
            RebuildGizmoTargets();
            return;
        }
    }
    ImGui::SameLine();
    bool active = group->IsActive();
    if (ImGui::Checkbox("有効##Group", &active)) group->SetActive(active);
    ImGui::SameLine();
    Vector3 gpos = group->GetPosition();
    float   posArr[3] = { gpos.x, gpos.y, gpos.z };
    ImGui::SetNextItemWidth(180);
    if (ImGui::DragFloat3("位置##G", posArr, 0.1f)) {
        group->SetPosition({ posArr[0], posArr[1], posArr[2] });
        RebuildGizmoTargets();
    }
    ImGui::SameLine();
    bool isGroupGizmo = (selectionMode_ == SelectionMode::Group && selectedGroupName_ == group->GetName());
    if (isGroupGizmo) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.36f, 0.53f, 1.0f));
    if (ImGui::SmallButton("ギズモ(グループ)")) SelectGroup(group->GetName());
    if (isGroupGizmo) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("このグループをギズモ対象に選択\n(すべてのエミッターを一緒に移動)");

    ImGui::Separator();

    // ================================================================
    // ★ グループ全体 Emit コントロールパネル
    // ================================================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.17f, 1.0f));
    ImGui::BeginChild("##EmitCtrlPanel", ImVec2(0, 78), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextColored({ 0.55f, 0.85f, 1.0f, 1.0f }, (std::string(Icon::Bullhorn) + "グループ全体 Emit").c_str());
    ImGui::Spacing();

    // x1
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.58f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.32f, 0.12f, 1.0f));
    if (ImGui::Button((std::string(Icon::Play) + "x1##EAG").c_str(), ImVec2(68, 0))) group->EmitAll(1);
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("全エミッターから 1 個ずつ発生");
    ImGui::SameLine();

    // x10
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.58f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.32f, 0.08f, 1.0f));
    if (ImGui::Button((std::string(Icon::Play) + "x10##EAG").c_str(), ImVec2(76, 0))) group->EmitAll(10);
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("全エミッターから 10 個ずつ発生");
    ImGui::SameLine();

    // x100 バースト
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.32f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.48f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.24f, 0.05f, 1.0f));
    if (ImGui::Button((std::string(Icon::Bolt) + "x100##EAG").c_str(), ImVec2(80, 0))) group->EmitAll(100);
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("全エミッターから一気に 100 個バースト発生");
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 自動 ON
    bool anyAutoOn = false;
    for (size_t i = 0; i < group->GetEmitterCount(); ++i) {
        auto* e = group->GetEmitter(i);
        if (e && e->GetAutoEmit()) { anyAutoOn = true; break; }
    }
    ImGui::PushStyleColor(ImGuiCol_Button,
        anyAutoOn ? ImVec4(0.15f, 0.35f, 0.55f, 1.0f) : ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.48f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.28f, 0.45f, 1.0f));
    if (ImGui::Button((std::string(Icon::Refresh) + "自動ON##AG").c_str(), ImVec2(90, 0))) group->SetAutoEmitAll(true);
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("全エミッターの自動発生を ON にする");
    ImGui::SameLine();

    // 自動 OFF
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.24f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.10f, 0.10f, 1.0f));
    if (ImGui::Button((std::string(Icon::Stop) + "自動OFF##AG").c_str(), ImVec2(96, 0))) group->SetAutoEmitAll(false);
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("全エミッターの自動発生を OFF にする");

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    ImGui::Separator();

    // ── エミッター一覧テーブル ────────────────────────────────
    ImGui::Text("エミッター (%zu個)", group->GetEmitterCount());
    ImGui::SameLine();
    ImGui::TextColored({ 0.5f, 0.7f, 1.0f, 1.0f }, "(Ctrl+[G]で複数ギズモ選択)");
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icon::PlusCircle) + "エミッターを追加##AddE").c_str()))
        ImGui::OpenPopup("AddEmitterPopup");

    // システム一覧選択ポップアップ
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("AddEmitterPopup")) {
        ImGui::Text((std::string(Icon::PlusCircle) + "エミッターを追加").c_str());
        ImGui::Separator();
        auto& pmgr = YParticleManager::GetInstance();
        auto sysNames = pmgr.GetAllSystemNames();
        if (!sysNames.empty()) {
            ImGui::Text("システムを選択:");
            std::string currentSel(newEmitterSystemName_);
            int visibleRows = static_cast<int>(std::min(sysNames.size(), size_t(6)));
            float listH = visibleRows * ImGui::GetTextLineHeightWithSpacing() + 6.0f;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginListBox("##SysPickList", ImVec2(-1, listH))) {
                for (const auto& sysName : sysNames) {
                    bool isSelected = (currentSel == sysName);
                    auto* sys = pmgr.GetSystem(sysName);
                    bool  connected = (sys != nullptr);
                    if (!connected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    if (ImGui::Selectable(sysName.c_str(), isSelected))
                        strncpy_s(newEmitterSystemName_, sysName.c_str(), sizeof(newEmitterSystemName_));
                    if (!connected) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered() && connected) {
                        size_t activeCnt = 0;
                        for (const auto& attr : sys->GetAttributes())
                            if (attr.isActive) activeCnt++;
                        ImGui::SetTooltip("最大: %u  有効: %zu", sys->GetMaxParticles(), activeCnt);
                    }
                }
                ImGui::EndListBox();
            }
        }
        else {
            ImGui::TextColored({ 1.0f, 0.7f, 0.0f, 1.0f },
                (std::string(Icon::Xmark) + "読み込み済みのシステムがありません").c_str());
            ImGui::TextDisabled("パーティクルシステムを先に作成してください");
        }
        ImGui::Separator();
        ImGui::Text("システム名 (直接入力):");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##NewESys", newEmitterSystemName_, sizeof(newEmitterSystemName_));
        if (newEmitterSystemName_[0] != '\0') {
            bool exists = (pmgr.GetSystem(newEmitterSystemName_) != nullptr);
            if (exists)
                ImGui::TextColored({ 0.2f, 1.0f, 0.3f, 1.0f },
                    (std::string(Icon::CheckCircle) + "\"%s\" が存在します").c_str(), newEmitterSystemName_);
            else
                ImGui::TextColored({ 1.0f, 0.6f, 0.1f, 1.0f },
                    (std::string(Icon::Xmark) + "\"%s\" は未登録です").c_str(), newEmitterSystemName_);
        }
        ImGui::Separator();
        ImGui::Text("ローカルオフセット:");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat3("##NewEOff", newEmitterOffset_, 0.1f);
        ImGui::Spacing();
        bool canAdd = (newEmitterSystemName_[0] != '\0');
        ImGui::BeginDisabled(!canAdd);
        if (ImGui::Button((std::string(Icon::PlusCircle) + "追加##EA").c_str(), ImVec2(120, 0))) {
            group->AddEmitter(newEmitterSystemName_,
                { newEmitterOffset_[0], newEmitterOffset_[1], newEmitterOffset_[2] });
            newEmitterSystemName_[0] = '\0';
            newEmitterOffset_[0] = newEmitterOffset_[1] = newEmitterOffset_[2] = 0.0f;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("キャンセル##EA", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    float tableHeight = ImGui::GetContentRegionAvail().y * 0.4f;

    if (ImGui::BeginTable("##EmitterTable", 6, tableFlags, ImVec2(0, tableHeight))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("システム", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("レート", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("数", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("自動", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("ギズモ", ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn("##Del", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();
        int deleteIndex = -1;
        for (size_t i = 0; i < group->GetEmitterCount(); ++i) {
            ImGui::TableNextRow();
            if (ShowEmitterRow(*group, i)) deleteIndex = (int)i;
        }
        if (deleteIndex >= 0) {
            group->RemoveEmitter(deleteIndex);

            // 削除されたインデックスより大きい要素を -1 して補正
            std::set<int> newSet;
            for (int idx : selectedEmitterIndices_) {
                if (idx < deleteIndex)      newSet.insert(idx);
                else if (idx > deleteIndex) newSet.insert(idx - 1);
                // idx == deleteIndex は削除済みなのでスキップ
            }
            selectedEmitterIndices_ = std::move(newSet);

            if (selectedEmitterIndex_ == deleteIndex)
                selectedEmitterIndex_ = -1;
            else if (selectedEmitterIndex_ > deleteIndex)
                selectedEmitterIndex_--;

            RebuildGizmoTargets();
        }
        ImGui::EndTable();
    }

    if (selectedEmitterIndex_ >= 0 && selectedEmitterIndex_ < (int)group->GetEmitterCount()) {
        auto* emitter = group->GetEmitter(selectedEmitterIndex_);
        if (emitter) {
            ImGui::Separator();
            ImGui::Text((std::string(Icon::Cog) + "詳細: エミッター [%d]  システム: %s").c_str(),
                selectedEmitterIndex_, emitter->GetSystemName().c_str());
            ImGui::Separator();
            ShowSelectedEmitterDetail(*emitter);
        }
    }
}

bool YEmitterGroupEditor::ShowEmitterRow(YEmitterGroup& group, size_t index) {
    auto* emitter = group.GetEmitter(index);
    if (!emitter) return false;
    ImGui::PushID((int)index);
    bool deleteRequested = false;
    bool isGizmoSelected = (selectionMode_ == SelectionMode::Emitter &&
        selectedEmitterIndices_.count((int)index) > 0);

    // ★ SpanAllColumns を使うとゴミ箱ボタンのクリックを横取りするため除去
    // 代わりに TableSetBgColor で行ハイライトを再現する
    bool isRowSelected = (selectedEmitterIndex_ == (int)index);
    if (isRowSelected) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
            ImGui::GetColorU32(ImVec4(0.17f, 0.36f, 0.53f, 0.45f)));
    }

    ImGui::TableSetColumnIndex(0);
    if (ImGui::Selectable(emitter->GetSystemName().c_str(), isRowSelected,
        ImGuiSelectableFlags_None))
        selectedEmitterIndex_ = (int)index;

    ImGui::TableSetColumnIndex(1);
    float rate = emitter->GetEmissionRate();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##Rate", &rate, 1.0f, 0.0f, 10000.0f, "%.1f"))
        emitter->SetEmissionRate(rate);

    ImGui::TableSetColumnIndex(2);
    int cnt = emitter->GetEmitCount();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##Cnt", &cnt, 1, 1, 9999))
        emitter->SetEmitCount(cnt);

    ImGui::TableSetColumnIndex(3);
    bool autoEmit = emitter->GetAutoEmit();
    if (ImGui::Checkbox("##Auto", &autoEmit)) emitter->SetAutoEmit(autoEmit);

    ImGui::TableSetColumnIndex(4);
    if (isGizmoSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.36f, 0.53f, 1.0f));
    if (ImGui::SmallButton("G##Giz"))
        SelectEmitter(group.GetName(), (int)index, ImGui::GetIO().KeyCtrl);
    if (isGizmoSelected) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("ギズモ対象に選択  (Ctrl+クリック: 複数選択)");

    ImGui::TableSetColumnIndex(5);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    if (ImGui::SmallButton((std::string(Icon::Trash) + "##Del").c_str())) deleteRequested = true;
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("このエミッターを削除");

    ImGui::PopID();
    return deleteRequested;
}

void YEmitterGroupEditor::ShowSelectedEmitterDetail(YParticleEmitter& emitter) {
    ImGui::Columns(2, "##EmitterDetailCols", false);
    ImGui::SetColumnWidth(0, 160);

    ImGui::Text("システム名");
    ImGui::NextColumn();
    char sysBuf[128];
    strncpy_s(sysBuf, emitter.GetSystemName().c_str(), sizeof(sysBuf));
    ImGui::SetNextItemWidth(-40);
    if (ImGui::InputText("##SysName", sysBuf, sizeof(sysBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto& pmgr = YParticleManager::GetInstance();
        if (!pmgr.GetSystem(sysBuf)) pmgr.CreateSystem(sysBuf, 1000);
        emitter.SetSystemName(sysBuf);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton((std::string(Icon::Cog) + "##SysPick").c_str())) ImGui::OpenPopup("##DetailSysPick");
    if (ImGui::BeginPopup("##DetailSysPick")) {
        ImGui::Text((std::string(Icon::Cog) + "システム選択:").c_str());
        auto& pmgr2 = YParticleManager::GetInstance();
        auto sysNames2 = pmgr2.GetAllSystemNames();
        if (sysNames2.empty()) {
            ImGui::TextDisabled("システムがありません");
        }
        else {
            int visRows = static_cast<int>(std::min(sysNames2.size(), size_t(8)));
            float lh = visRows * ImGui::GetTextLineHeightWithSpacing() + 6.0f;
            if (ImGui::BeginListBox("##DetailSysLB", ImVec2(240, lh))) {
                for (const auto& sn : sysNames2) {
                    bool isSel = (emitter.GetSystemName() == sn);
                    if (ImGui::Selectable(sn.c_str(), isSel)) {
                        emitter.SetSystemName(sn);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::NextColumn();

    ImGui::Text("有効");           ImGui::NextColumn();
    bool act = emitter.IsActive();
    if (ImGui::Checkbox("##EAct", &act)) emitter.SetActive(act);
    ImGui::NextColumn();

    ImGui::Text("自動発生");       ImGui::NextColumn();
    bool autoE = emitter.GetAutoEmit();
    if (ImGui::Checkbox("##EAuto", &autoE)) emitter.SetAutoEmit(autoE);
    ImGui::NextColumn();

    ImGui::Text("レート (毎秒)");  ImGui::NextColumn();
    float rate = emitter.GetEmissionRate();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##ERate", &rate, 0.5f, 0.0f, 10000.0f))
        emitter.SetEmissionRate(rate);
    ImGui::NextColumn();

    ImGui::Text("発生数");         ImGui::NextColumn();
    int cnt = emitter.GetEmitCount();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##ECnt", &cnt, 1, 1, 9999)) emitter.SetEmitCount(cnt);
    ImGui::NextColumn();

    ImGui::Text("位置");           ImGui::NextColumn();
    Vector3 pos = emitter.GetPosition();
    float   posArr[3] = { pos.x, pos.y, pos.z };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##EPos", posArr, 0.1f)) {
        emitter.SetPosition({ posArr[0], posArr[1], posArr[2] });
        RebuildGizmoTargets();
    }
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Spacing();

    // 形状設定
    ImGui::Text("形状"); ImGui::SameLine();
    int currentType = 0;
    if (auto* shape = emitter.GetShape()) {
        switch (shape->GetType()) {
        case YEmitterShape::Type::Sphere: currentType = 1; break;
        case YEmitterShape::Type::Box:    currentType = 2; break;
        default:                          currentType = 0; break;
        }
    }
    const char* shapeNames[] = { "点", "球", "ボックス", "コーン" };
    if (auto* shape = emitter.GetShape()) {
        switch (shape->GetType()) {
        case YEmitterShape::Type::Sphere: currentType = 1; break;
        case YEmitterShape::Type::Box:    currentType = 2; break;
        case YEmitterShape::Type::Cone:   currentType = 3; break;
        default:                          currentType = 0; break;
        }
    }
    ImGui::SetNextItemWidth(120);
    if (ImGui::Combo("##EShape", &currentType, shapeNames, 4)) {
        switch (currentType) {
        case 0: emitter.SetShapePoint();           break;
        case 1: emitter.SetShapeSphere(1.0f);      break;
        case 2: emitter.SetShapeBox({ 1, 1, 1 });  break;
        case 3: emitter.SetShapeCone(25.0f, 2.0f); break;
        }
    }
    // 形状パラメーター（DrawEditor() に委譲）
    if (auto* shape = emitter.GetShape()) {
        ImGui::Indent();
        shape->DrawEditor();
        ImGui::Unindent();
    }

    // 手動発生
    ImGui::Spacing();
    ImGui::Text("手動発生 (このエミッター):"); ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.58f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.32f, 0.12f, 1.0f));
    if (ImGui::SmallButton((std::string(Icon::Play) + " x1##ED").c_str()))  emitter.Emit(1);
    ImGui::SameLine();
    if (ImGui::SmallButton((std::string(Icon::Play) + " x10##ED").c_str())) emitter.Emit(10);
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.32f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.48f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.24f, 0.05f, 1.0f));
    if (ImGui::SmallButton((std::string(Icon::Bolt) + " x100##ED").c_str())) emitter.EmitBurst(100);
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    auto* sys = emitter.GetTargetSystem();
    if (sys) {
        size_t activeCnt = 0;
        for (const auto& attr : sys->GetAttributes())
            if (attr.isActive) activeCnt++;
        ImGui::TextColored({ 0.2f, 1.0f, 0.3f, 1.0f },
            (std::string(Icon::CheckCircle) + "  接続済み  最大:%u  有効:%zu").c_str(),
            sys->GetMaxParticles(), activeCnt);
    }
    else {
        ImGui::TextColored({ 1.0f, 0.4f, 0.0f, 1.0f },
            (std::string(Icon::Xmark) + " システム \"%s\" が見つかりません").c_str(),
            emitter.GetSystemName().c_str());
    }
}

void YEmitterGroupEditor::ShowFileButtons() {
    ImGui::Text("ファイル");
    if (ImGui::Button((std::string(Icon::FloppyDisk) + "すべて保存##FG").c_str(), ImVec2(-1, 0))) {
        saveBrowser_.Scan();
        showSavePopup_ = true;
    }
    if (showSavePopup_) ImGui::OpenPopup("##SaveEmitterGroups");
    ImGui::SetNextWindowSize(ImVec2(480, 380), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##SaveEmitterGroups", &showSavePopup_,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::Text((std::string(Icon::FloppyDisk) + " 保存 — ファイルをクリックして上書き、または下に新しい名前を入力").c_str());
        ImGui::Separator();
        saveBrowser_.Draw("##SaveBrowserChild", ImVec2(0, 260));
        ImGui::Separator();
        static char saveAsName[256] = "";
        ImGui::SetNextItemWidth(-80);
        ImGui::InputTextWithHint("##SaveAsName", "新規ファイル.json", saveAsName, sizeof(saveAsName));
        ImGui::SameLine();
        if (ImGui::Button("名前を付けて保存")) {
            if (saveAsName[0] != '\0') {
                std::string path = saveBrowser_.GetCurrentDir() + saveAsName;
                bool ok = YEmitterGroupManager::GetInstance().SaveAllToFile(path);
                SetNotification(ok, ok ? "保存完了: " + path : "保存失敗: " + path);
                saveAsName[0] = '\0';
                showSavePopup_ = false;
                ImGui::CloseCurrentPopup();
            }
        }
        if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
            showSavePopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::Button((std::string(Icon::FolderOpen) + "すべて読み込み##FG").c_str(), ImVec2(-1, 0))) {
        loadBrowser_.Scan();
        showLoadPopup_ = true;
    }
    if (showLoadPopup_) ImGui::OpenPopup("##LoadEmitterGroups");
    ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##LoadEmitterGroups", &showLoadPopup_,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::Text((std::string(Icon::FolderOpen) + "読み込み — JSONファイルを選択してください").c_str());
        ImGui::Separator();
        loadBrowser_.Draw("##LoadBrowserChild", ImVec2(0, 280));
        ImGui::Separator();
        if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
            showLoadPopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

#endif // USE_IMGUI
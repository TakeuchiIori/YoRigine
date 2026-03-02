#ifdef USE_IMGUI

#include "YEmitterGroupEditor.h"
#include "imgui.h"
#include <Editor/Editor.h>

//=================================================================
// 内部ヘルパー：ドラッグ可能な垂直分割線（既存コードそのまま）
//=================================================================

static void Splitter(float thickness, float* size1, float min_size1, float min_size2, float height = -1.0f)
{
    ImVec2 backup_pos = ImGui::GetCursorPos();
    if (height < 0)
        height = ImGui::GetContentRegionAvail().y;

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
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SetCursorPos(backup_pos);
}

//=================================================================
// コンストラクタ
//=================================================================

YEmitterGroupEditor::YEmitterGroupEditor()
    : saveBrowser_(emitterDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
    , loadBrowser_(emitterDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
{
    gizmoCtrl_.Initialize();

    saveBrowser_.SetOnFileSelected([this](const std::string& path) {
        YEmitterGroupManager::GetInstance().SaveAllToFile(path);
        showSavePopup_ = false;
        });

    loadBrowser_.SetOnFileSelected([this](const std::string& path) {
        YEmitterGroupManager::GetInstance().LoadAllFromFile(path);
        showLoadPopup_ = false;
        });
}

//=================================================================
// DrawGizmo — ビューポート上にギズモを描画
// ImGui::Image の直後・同一ウィンドウ内で呼ぶ
//=================================================================

void YEmitterGroupEditor::DrawGizmo()
{
    if (!camera_ || gizmoTargets_.empty()) return;

    gizmoCtrl_.Draw(
        camera_,
        gizmoTargets_,
        Editor::GetInstance()->GetGameViewPos(),
        Editor::GetInstance()->GetGameViewSize());
}

//=================================================================
// ギズモ選択 — グループ単位
//=================================================================

void YEmitterGroupEditor::SelectGroup(const std::string& groupName)
{
    selectedGroupName_ = groupName;
    selectedEmitterIndices_.clear();
    selectedEmitterIndex_ = -1;
    selectionMode_ = SelectionMode::Group;
    RebuildGizmoTargets();
}

//=================================================================
// ギズモ選択 — 個別エミッター（Ctrl で複数追加）
//=================================================================

void YEmitterGroupEditor::SelectEmitter(
    const std::string& groupName, int index, bool multiSelect)
{
    // グループが変わったときは選択リセット
    if (groupName != selectedGroupName_) {
        selectedGroupName_ = groupName;
        selectedEmitterIndices_.clear();
    }

    selectionMode_ = SelectionMode::Emitter;

    if (multiSelect) {
        // トグル
        if (selectedEmitterIndices_.count(index))
            selectedEmitterIndices_.erase(index);
        else
            selectedEmitterIndices_.insert(index);
    }
    else {
        selectedEmitterIndices_.clear();
        selectedEmitterIndices_.insert(index);
    }

    selectedEmitterIndex_ = index;  // Detail 表示用
    RebuildGizmoTargets();
}

//=================================================================
// RebuildGizmoTargets — 選択状態に合わせてアダプタを再構築
//=================================================================

void YEmitterGroupEditor::RebuildGizmoTargets()
{
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
                emitter,
                selectedGroupName_ + "[" + std::to_string(idx) + "]");
            gizmoTargets_.push_back(gizmable.get());
            emitterGizmables_.push_back(std::move(gizmable));
        }
    }
}

//=================================================================
// ShowEditor — メイン描画エントリポイント
//=================================================================

void YEmitterGroupEditor::ShowEditor()
{
    // ギズモモード切替ツールバー（上部に常時表示）
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

//=================================================================
// ShowGizmoToolbar — T/R/S 切替 + 選択状態の表示
//=================================================================

void YEmitterGroupEditor::ShowGizmoToolbar()
{
    gizmoCtrl_.DrawSettings();
}

//=================================================================
// ShowGroupList — 左ペイン（グループ選択でギズモと連動）
//=================================================================

void YEmitterGroupEditor::ShowGroupList()
{
    ImGui::Text("エミッターグループ");
    ImGui::Separator();

    auto& mgr = YEmitterGroupManager::GetInstance();
    auto  names = mgr.GetAllGroupNames();

    for (const auto& name : names) {
        auto* group = mgr.GetGroup(name);
        bool  active = group ? group->IsActive() : false;

        if (!active)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        bool isSelected = (name == selectedGroupName_);
        if (ImGui::Selectable(name.c_str(), isSelected))
            SelectGroup(name);  // クリックでグループギズモに切替

        if (!active) ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem(name.c_str())) {
            if (ImGui::MenuItem("グループギズモを選択"))
                SelectGroup(name);
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

//=================================================================
// ShowCreateGroupUI — 既存コードそのまま
//=================================================================

void YEmitterGroupEditor::ShowCreateGroupUI()
{
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

//=================================================================
// ShowGroupDetail — 右ペイン（エミッター選択をギズモと連動）
//=================================================================

void YEmitterGroupEditor::ShowGroupDetail()
{
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

    // ── グループ名・有効フラグ ────────────────────────────────
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
    if (ImGui::Checkbox("有効##Group", &active))
        group->SetActive(active);

    // グループ位置（ギズモと同期・手動入力も可）
    ImGui::SameLine();
    Vector3 gpos = group->GetPosition();
    float   posArr[3] = { gpos.x, gpos.y, gpos.z };
    ImGui::SetNextItemWidth(180);
    if (ImGui::DragFloat3("位置##G", posArr, 0.1f)) {
        group->SetPosition({ posArr[0], posArr[1], posArr[2] });
        RebuildGizmoTargets();
    }

    // グループ全体をギズモ対象にするボタン
    ImGui::SameLine();
    bool isGroupGizmo = (selectionMode_ == SelectionMode::Group &&
        selectedGroupName_ == group->GetName());
    if (isGroupGizmo)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.36f, 0.53f, 1.0f));
    if (ImGui::SmallButton("ギズモ(グループ)"))
        SelectGroup(group->GetName());
    if (isGroupGizmo)
        ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("このグループをギズモ対象に選択\n(すべてのエミッターを一緒に移動)");

    ImGui::SameLine();
    if (ImGui::SmallButton("自動ON##G"))  group->SetAutoEmitAll(true);
    ImGui::SameLine();
    if (ImGui::SmallButton("自動OFF##G")) group->SetAutoEmitAll(false);

    ImGui::Separator();

    // ── エミッター一覧テーブル ────────────────────────────────
    ImGui::Text("エミッター (%zu個)", group->GetEmitterCount());
    ImGui::SameLine();
    ImGui::TextColored({ 0.5f, 0.7f, 1.0f, 1.0f }, "(Ctrl+[G]で複数ギズモ選択)");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ エミッターを追加"))
        ImGui::OpenPopup("AddEmitterPopup");

    if (ImGui::BeginPopup("AddEmitterPopup")) {
        ImGui::Text("システム名:");
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##NewESys", newEmitterSystemName_, sizeof(newEmitterSystemName_));
        ImGui::Text("ローカルオフセット:");
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat3("##NewEOff", newEmitterOffset_, 0.1f);
        if (ImGui::Button("追加")) {
            if (newEmitterSystemName_[0] != '\0') {
                group->AddEmitter(newEmitterSystemName_,
                    { newEmitterOffset_[0], newEmitterOffset_[1], newEmitterOffset_[2] });
                newEmitterSystemName_[0] = '\0';
                newEmitterOffset_[0] = newEmitterOffset_[1] = newEmitterOffset_[2] = 0;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    float tableHeight = ImGui::GetContentRegionAvail().y * 0.4f;

    // ★ Gizmo 列を1列追加（元は5列 → 6列）
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
            if (ShowEmitterRow(*group, i))
                deleteIndex = (int)i;
        }

        if (deleteIndex >= 0) {
            group->RemoveEmitter(deleteIndex);
            selectedEmitterIndices_.erase(deleteIndex);
            if (selectedEmitterIndex_ == deleteIndex) selectedEmitterIndex_ = -1;
            RebuildGizmoTargets();
        }
        ImGui::EndTable();
    }

    // ── 選択エミッターの詳細設定 ──────────────────────────────
    if (selectedEmitterIndex_ >= 0 &&
        selectedEmitterIndex_ < (int)group->GetEmitterCount())
    {
        auto* emitter = group->GetEmitter(selectedEmitterIndex_);
        if (emitter) {
            ImGui::Separator();
            ImGui::Text("詳細: エミッター [%d]  システム: %s",
                selectedEmitterIndex_, emitter->GetSystemName().c_str());
            ImGui::Separator();
            ShowSelectedEmitterDetail(*emitter);
        }
    }
}

//=================================================================
// ShowEmitterRow — テーブル行1件（Gizmo列を追加）
//=================================================================

bool YEmitterGroupEditor::ShowEmitterRow(YEmitterGroup& group, size_t index)
{
    auto* emitter = group.GetEmitter(index);
    if (!emitter) return false;

    ImGui::PushID((int)index);

    bool deleteRequested = false;
    bool isGizmoSelected = (selectionMode_ == SelectionMode::Emitter &&
        selectedEmitterIndices_.count((int)index) > 0);

    // ── System 列（クリックで Detail 表示のみ・ギズモには影響しない）──
    ImGui::TableSetColumnIndex(0);
    bool isRowSelected = (selectedEmitterIndex_ == (int)index);
    if (ImGui::Selectable(emitter->GetSystemName().c_str(), isRowSelected,
        ImGuiSelectableFlags_SpanAllColumns))
    {
        selectedEmitterIndex_ = (int)index;  // Detail 表示用のみ更新
    }

    // ── Rate 列 ───────────────────────────────────────────────
    ImGui::TableSetColumnIndex(1);
    float rate = emitter->GetEmissionRate();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##Rate", &rate, 1.0f, 0.0f, 10000.0f, "%.1f"))
        emitter->SetEmissionRate(rate);

    // ── Count 列 ──────────────────────────────────────────────
    ImGui::TableSetColumnIndex(2);
    int cnt = emitter->GetEmitCount();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##Cnt", &cnt, 1, 1, 9999))
        emitter->SetEmitCount(cnt);

    // ── Auto 列 ───────────────────────────────────────────────
    ImGui::TableSetColumnIndex(3);
    bool autoEmit = emitter->GetAutoEmit();
    if (ImGui::Checkbox("##Auto", &autoEmit))
        emitter->SetAutoEmit(autoEmit);

    // ── Gizmo 列（★ 追加） ────────────────────────────────────
    ImGui::TableSetColumnIndex(4);
    if (isGizmoSelected)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.36f, 0.53f, 1.0f));

    if (ImGui::SmallButton("G##Giz"))
        SelectEmitter(group.GetName(), (int)index, ImGui::GetIO().KeyCtrl);

    if (isGizmoSelected)
        ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ギズモ対象に選択  (Ctrl+クリック: 複数選択)");

    // ── Delete 列 ─────────────────────────────────────────────
    ImGui::TableSetColumnIndex(5);
    if (ImGui::SmallButton("x##Del"))
        deleteRequested = true;

    ImGui::PopID();
    return deleteRequested;
}

//=================================================================
// ShowSelectedEmitterDetail — 既存コードそのまま
//=================================================================

void YEmitterGroupEditor::ShowSelectedEmitterDetail(YParticleEmitter& emitter)
{
    ImGui::Columns(2, "##EmitterDetailCols", false);
    ImGui::SetColumnWidth(0, 160);

    ImGui::Text("システム名");
    ImGui::NextColumn();
    char sysBuf[128];
    strncpy_s(sysBuf, emitter.GetSystemName().c_str(), sizeof(sysBuf));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##SysName", sysBuf, sizeof(sysBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto& pmgr = YParticleManager::GetInstance();
        if (!pmgr.GetSystem(sysBuf)) pmgr.CreateSystem(sysBuf, 1000);
        emitter.SetSystemName(sysBuf);
    }
    ImGui::NextColumn();

    ImGui::Text("有効");
    ImGui::NextColumn();
    bool act = emitter.IsActive();
    if (ImGui::Checkbox("##EAct", &act)) emitter.SetActive(act);
    ImGui::NextColumn();

    ImGui::Text("自動発生");
    ImGui::NextColumn();
    bool autoE = emitter.GetAutoEmit();
    if (ImGui::Checkbox("##EAuto", &autoE)) emitter.SetAutoEmit(autoE);
    ImGui::NextColumn();

    ImGui::Text("レート (毎秒)");
    ImGui::NextColumn();
    float rate = emitter.GetEmissionRate();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##ERate", &rate, 0.5f, 0.0f, 10000.0f))
        emitter.SetEmissionRate(rate);
    ImGui::NextColumn();

    ImGui::Text("発生数");
    ImGui::NextColumn();
    int cnt = emitter.GetEmitCount();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##ECnt", &cnt, 1, 1, 9999))
        emitter.SetEmitCount(cnt);
    ImGui::NextColumn();

    ImGui::Text("位置");
    ImGui::NextColumn();
    Vector3 pos = emitter.GetPosition();
    float   posArr[3] = { pos.x, pos.y, pos.z };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##EPos", posArr, 0.1f)) {
        emitter.SetPosition({ posArr[0], posArr[1], posArr[2] });
        RebuildGizmoTargets();  // 手動変更時もギズモ位置に反映
    }
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::Spacing();

    // ── 形状設定 ──────────────────────────────────────────────
    ImGui::Text("形状");
    ImGui::SameLine();

    int currentType = 0;
    if (auto* shape = emitter.GetShape()) {
        switch (shape->GetType()) {
        case YEmitterShape::Type::Sphere: currentType = 1; break;
        case YEmitterShape::Type::Box:    currentType = 2; break;
        default:                          currentType = 0; break;
        }
    }
    const char* shapeNames[] = { "点", "球", "ボックス" };
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("##EShape", &currentType, shapeNames, 3)) {
        switch (currentType) {
        case 0: emitter.SetShapePoint();          break;
        case 1: emitter.SetShapeSphere(1.0f);     break;
        case 2: emitter.SetShapeBox({ 1, 1, 1 }); break;
        }
    }

    if (auto* shape = emitter.GetShape()) {
        ImGui::Indent();
        switch (shape->GetType()) {
        case YEmitterShape::Type::Sphere: {
            auto* s = static_cast<YEmitterSphere*>(shape);
            ImGui::DragFloat("半径##ES", &s->radius, 0.05f, 0.0f, 1000.0f);
            ImGui::Checkbox("シェルのみ##ES", &s->shellOnly);
            break;
        }
        case YEmitterShape::Type::Box: {
            auto* b = static_cast<YEmitterBox*>(shape);
            ImGui::DragFloat3("サイズ##EB", &b->size.x, 0.05f, 0.0f, 1000.0f);
            break;
        }
        default: break;
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Text("手動発生:");
    ImGui::SameLine();
    if (ImGui::SmallButton("x1"))   emitter.Emit(1);
    ImGui::SameLine();
    if (ImGui::SmallButton("x10"))  emitter.Emit(10);
    ImGui::SameLine();
    if (ImGui::SmallButton("x100")) emitter.EmitBurst(100);

    ImGui::Spacing();
    auto* sys = emitter.GetTargetSystem();
    if (sys) {
        size_t activeCnt = 0;
        for (const auto& attr : sys->GetAttributes())
            if (attr.isActive) activeCnt++;
        ImGui::TextColored({ 0, 1, 0, 1 }, "接続済み  最大:%u  有効:%zu",
            sys->GetMaxParticles(), activeCnt);
    }
    else {
        ImGui::TextColored({ 1, 0.4f, 0, 1 },
            "システム \"%s\" が見つかりません", emitter.GetSystemName().c_str());
    }
}

//=================================================================
// ShowFileButtons — 既存コードそのまま
//=================================================================

void YEmitterGroupEditor::ShowFileButtons()
{
    ImGui::Text("ファイル");

    if (ImGui::Button("すべて保存", ImVec2(-1, 0))) {
        saveBrowser_.Scan();
        showSavePopup_ = true;
    }
    if (showSavePopup_) ImGui::OpenPopup("##SaveEmitterGroups");

    ImGui::SetNextWindowSize(ImVec2(480, 380), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##SaveEmitterGroups", &showSavePopup_,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("保存 — ファイルをクリックして上書き、または下に新しい名前を入力");
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
                YEmitterGroupManager::GetInstance().SaveAllToFile(path);
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

    if (ImGui::Button("すべて読み込み", ImVec2(-1, 0))) {
        loadBrowser_.Scan();
        showLoadPopup_ = true;
    }
    if (showLoadPopup_) ImGui::OpenPopup("##LoadEmitterGroups");

    ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##LoadEmitterGroups", &showLoadPopup_,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("読み込み — JSONファイルを選択してください");
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
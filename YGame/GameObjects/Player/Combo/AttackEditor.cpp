#include "AttackEditor.h"
#include <algorithm>

#include <Debugger/Logger.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

using json = nlohmann::json;

AttackDataEditor::AttackDataEditor()
{
    attacks_ = &AttackDatabase::Get();
    std::fill(std::begin(nameBuffer_), std::end(nameBuffer_), '\0');
}

void AttackDataEditor::SetTarget(std::vector<AttackData>* list)
{
    attacks_      = list;
    currentIndex_ = (attacks_ && !attacks_->empty()) ? 0 : -1;
    prevIndex_    = -1;
}

void AttackDataEditor::SetFilePath(const std::string& path)
{
    filePath_ = path;
    std::string msg = "[AttackEditor] File path set to: " + filePath_ + "\n";
    Logger(msg.c_str());
}

// ★ 追加
void AttackDataEditor::SetFrameFilePath(const std::string& path)
{
    frameFilePath_ = path;
    std::string msg = "[AttackEditor] Frame file path set to: " + frameFilePath_ + "\n";
    Logger(msg.c_str());
}

void AttackDataEditor::SetReloadCallback(std::function<void()> callback)
{
    onReloadCallback_ = callback;
}

//=============================================================================
// DrawImGui  ── レイアウト全体
//=============================================================================
void AttackDataEditor::DrawImGui()
{
#ifdef USE_IMGUI
    DrawToolbar();
    ImGui::Separator();

    // ── 上段：攻撃リスト | プロパティインスペクタ ──
    ImGui::Columns(2, nullptr, true);
    DrawAttackList();
    ImGui::NextColumn();
    DrawAttackDetail();
    ImGui::Columns(1);

    ImGui::Separator();

    // ── 下段：ドープシート ──（★ 追加）
    DrawDopeSheet();
#endif
}

//=============================================================================
// DrawDopeSheet  ── ★ 新規
//=============================================================================
void AttackDataEditor::DrawDopeSheet()
{
#ifdef USE_IMGUI
    if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
    {
        ImGui::TextDisabled("攻撃を選択するとタイムラインが表示されます");
        return;
    }

    // 選択が変わったら BuildTracks を呼ぶ
    if (currentIndex_ != prevIndex_)
    {
        OnAttackSelected();
        prevIndex_ = currentIndex_;
    }

    const AttackData& attack = attacks_->at(currentIndex_);

    // ドープシートのヘッダー
    ImGui::Text("タイムライン : %s", attack.animationName.c_str());

    // fps と totalFrames をインラインで編集できるようにする
    AttackFrameData& fd = GetOrCreateFrameData();
    bool frameChanged = false;
    ImGui::SetNextItemWidth(80.0f);
    frameChanged |= ImGui::InputInt("総フレーム数", &fd.totalFrames);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    frameChanged |= ImGui::InputInt("FPS", &fd.fps);
    fd.totalFrames = std::max(1,  fd.totalFrames);
    fd.fps         = std::max(1,  fd.fps);

    ImGui::Separator();

    // DopeSheetEditor 本体を描画
    // Draw() が true を返したらトラックが編集されたので書き戻す
    bool dopeChanged = dopeEditor_.Draw(
        "##combo_dope",
        dopeTracks_,
        fd.totalFrames,
        fd.fps
    );

    if (dopeChanged || frameChanged)
    {
        // tracks → AttackFrameData
        AttackFrameConverter::ApplyTracks(dopeTracks_, fd);

        // AttackFrameData → AttackData（秒単位フィールド）
        AttackData& atk = attacks_->at(currentIndex_);
        AttackFrameConverter::SyncToAttackData(fd, atk);

        if (autoReload_)
        {
            SaveFrameDataToJson();
            SaveToJson();
            TriggerReload();
        }
    }
#endif
}

//=============================================================================
// OnAttackSelected  ── ★新規
// 攻撃を選択したとき呼ばれる。FrameData を取得して BuildTracks する
//=============================================================================
void AttackDataEditor::OnAttackSelected()
{
    if (!attacks_ || currentIndex_ < 0) return;

    const AttackData& attack = attacks_->at(currentIndex_);
    AttackFrameData&  fd     = GetOrCreateFrameData();

    // AttackFrameData → DopeTrack リスト
    dopeTracks_ = AttackFrameConverter::BuildTracks(fd);

    // シークバーをリセット
    dopeEditor_.ResetView();

    std::string msg = "[AttackEditor] Tracks built for: " + attack.name + "\n";
    Logger(msg.c_str());
}

//=============================================================================
// GetOrCreateFrameData  ── ★ 新規
//=============================================================================
AttackFrameData& AttackDataEditor::GetOrCreateFrameData()
{
    const std::string& name = attacks_->at(currentIndex_).name;
    return AttackFrameDatabase::FindOrCreate(name);
}

//=============================================================================
// LoadFrameDataFromJson / SaveFrameDataToJson  ── ★ 新規
//=============================================================================
void AttackDataEditor::LoadFrameDataFromJson()
{
    AttackFrameDatabase::LoadFromFile(frameFilePath_);

    // 選択中の攻撃のトラックも再構築する
    if (currentIndex_ >= 0)
        OnAttackSelected();
}

void AttackDataEditor::SaveFrameDataToJson()
{
    AttackFrameDatabase::SaveToFile(frameFilePath_);
}

//=============================================================================
// 以下は既存コードそのまま
//=============================================================================

void AttackDataEditor::DrawToolbar()
{
#ifdef USE_IMGUI
    if (ImGui::Button("保存"))
    {
        SaveToJson();
        SaveFrameDataToJson(); // ★ フレームデータも一緒に保存
        TriggerReload();
    }
    ImGui::SameLine();
    if (ImGui::Button("読み込み"))
    {
        LoadFromJson();
        LoadFrameDataFromJson(); // ★ フレームデータも一緒に読み込み
        TriggerReload();
    }
    ImGui::SameLine();
    if (ImGui::Button("保存 & リロード"))
    {
        SaveToJson();
        SaveFrameDataToJson(); // ★
        TriggerReload();
    }

    ImGui::SameLine();
    ImGui::Text("ファイル: %s", filePath_.c_str());
    ImGui::SameLine();
    ImGui::Text("| 攻撃数: %d", attacks_ ? static_cast<int>(attacks_->size()) : 0);
    ImGui::Separator();

    if (ImGui::Checkbox("編集時に自動リロード", &autoReload_))
    {
        Logger(autoReload_
            ? "[AttackEditor] 自動リロードが有効になりました\n"
            : "[AttackEditor] 自動リロードが無効になりました\n");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("編集時に攻撃設定を自動的にリロードします");
        ImGui::EndTooltip();
    }
#endif
}

void AttackDataEditor::DrawAttackList()
{
#ifdef USE_IMGUI
    if (!attacks_)
    {
        ImGui::Text("攻撃リストがありません。");
        return;
    }

    ImGui::Text("攻撃数 (%d)", static_cast<int>(attacks_->size()));
    ImGui::Separator();

    std::map<AttackType, std::vector<int>> categorizedAttacks;
    for (int i = 0; i < static_cast<int>(attacks_->size()); ++i)
        categorizedAttacks[attacks_->at(i).type].push_back(i);

    static const char* attackTypes[] = { "A技 (軽)", "B技 (重)", "奥義 (究極)" };

    for (int typeIndex = 0; typeIndex < 3; ++typeIndex)
    {
        AttackType type = static_cast<AttackType>(typeIndex);
        if (ImGui::CollapsingHeader(attackTypes[typeIndex], ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int i : categorizedAttacks[type])
            {
                const bool isSelected = (i == currentIndex_);
                const std::string label = attacks_->at(i).name + "##attack_" + std::to_string(i);

                if (isSelected)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));

                if (ImGui::Selectable(label.c_str(), isSelected))
                    currentIndex_ = i;

                if (isSelected)
                    ImGui::PopStyleColor();
            }
        }
    }

    ImGui::Separator();
    if (ImGui::Button("新規作成"))  { NewAttack();       if (autoReload_) TriggerReload(); }
    ImGui::SameLine();
    if (ImGui::Button("複製"))      { DuplicateAttack(); if (autoReload_) TriggerReload(); }
    ImGui::SameLine();
    if (ImGui::Button("削除"))      { DeleteAttack();    if (autoReload_) TriggerReload(); }
#endif
}

void AttackDataEditor::DrawAttackDetail()
{
#ifdef USE_IMGUI
    if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
    {
        ImGui::Text("攻撃が選択されていません。");
        return;
    }

    AttackData& attack = attacks_->at(currentIndex_);
    static const char* attackTypes[] = { "A技 (軽)", "B技 (重)", "奥義 (究極)" };

    ImGui::Text("詳細");
    ImGui::Separator();

    bool changed = false;

    // 名前
    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", attack.name.c_str());
    if (ImGui::InputText("名前", nameBuffer_, sizeof(nameBuffer_)))
    {
        attack.name = nameBuffer_;
        changed = true;
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_None))
    {
        char animBuffer[256];
        std::snprintf(animBuffer, sizeof(animBuffer), "%s", attack.animationName.c_str());
        if (ImGui::InputText("アニメーション名", animBuffer, sizeof(animBuffer)))
        {
            attack.animationName = animBuffer;
            changed = true;
        }
        int currentType = static_cast<int>(attack.type);
        if (ImGui::Combo("タイプ", &currentType, attackTypes, 3))
        {
            attack.type = static_cast<AttackType>(currentType);
            changed = true;
        }
        char cameraEffectBuffer[256];
        std::snprintf(cameraEffectBuffer, sizeof(cameraEffectBuffer), "%s", attack.cameraEffect.c_str());
        if (ImGui::InputText("カメラ効果", cameraEffectBuffer, sizeof(cameraEffectBuffer)))
        {
            attack.cameraEffect = cameraEffectBuffer;
            changed = true;
        }
    }

    if (ImGui::CollapsingHeader("タイミング", ImGuiTreeNodeFlags_None))
    {
        changed |= ImGui::InputFloat("持続時間",       &attack.duration,       0.01f, 0.1f, "%.2f");
        changed |= ImGui::InputFloat("硬直時間",       &attack.recovery,       0.01f, 0.1f, "%.2f");
        changed |= ImGui::InputFloat("継続受付時間",   &attack.continueWindow, 0.01f, 0.1f, "%.2f");
        changed |= ImGui::InputFloat("モーション速度", &attack.motionSpeed,    0.01f, 0.1f, "%.2f");
    }

    if (ImGui::CollapsingHeader("ダメージ & 効果", ImGuiTreeNodeFlags_None))
    {
        changed |= ImGui::InputFloat("基本ダメージ",           &attack.baseDamage,        1.0f,  10.0f, "%.1f");
        changed |= ImGui::InputFloat("ノックバック",           &attack.knockback,         0.1f,  1.0f,  "%.1f");
        changed |= ImGui::InputFloat("ノックバック持続時間",   &attack.knockbackDuration, 0.1f,  1.0f,  "%.2f");
        changed |= ImGui::InputFloat3("攻撃範囲",              &attack.attackRange.x);
        changed |= ImGui::InputFloat("攻撃時のステップ距離",   &attack.stepDistance,      0.1f,  1.0f,  "%.2f");
    }

    if (ImGui::CollapsingHeader("CCシステム"))
    {
        changed |= ImGui::InputInt("CC消費",         &attack.ccCost);
        changed |= ImGui::InputInt("CCヒット時回復", &attack.ccOnHit);
    }

    if (ImGui::CollapsingHeader("コンボ特性"))
    {
        changed |= ImGui::Checkbox("キャンセル可能",   &attack.canCancel);
        changed |= ImGui::Checkbox("任意に連携可能",   &attack.canChainToAny);

        if (ImGui::TreeNode("推奨次攻撃"))
        {
            for (size_t i = 0; i < attack.preferredNext.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                int currentPreferred = static_cast<int>(attack.preferredNext[i]);
                if (ImGui::Combo(("##" + std::to_string(i)).c_str(), &currentPreferred, attackTypes, 3))
                {
                    attack.preferredNext[i] = static_cast<AttackType>(currentPreferred);
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("X"))
                {
                    attack.preferredNext.erase(attack.preferredNext.begin() + i);
                    changed = true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("推奨を追加"))
            {
                attack.preferredNext.push_back(AttackType::A_Arte);
                changed = true;
            }
            ImGui::TreePop();
        }
    }

    if (changed && autoReload_)
    {
        SaveToJson();
        TriggerReload();
    }
#endif
}

void AttackDataEditor::NewAttack()
{
    if (!attacks_) return;

    AttackData data;
    data.name             = "NewAttack_" + std::to_string(attacks_->size());
    data.animationName    = "Idle";
    data.type             = AttackType::A_Arte;
    data.duration         = 0.3f;
    data.recovery         = 0.2f;
    data.continueWindow   = 0.3f;
    data.baseDamage       = 30.0f;
    data.knockback        = 5.0f;
    data.knockbackDuration= 0.5f;
    data.attackRange      = { 2.0f, 1.0f, 1.5f };
    data.ccCost           = 1;
    data.ccOnHit          = 0;
    data.canCancel        = true;
    data.canChainToAny    = true;
    data.launches         = false;
    data.wallBounce       = false;
    data.groundBounce     = false;
    data.effect           = "";
    data.motionSpeed      = 1.0f;

    attacks_->push_back(data);
    currentIndex_ = static_cast<int>(attacks_->size()) - 1;
    prevIndex_    = -1; // ★ 強制的に BuildTracks させる

    // ★ 新規攻撃のフレームデータも生成しておく
    AttackFrameDatabase::FindOrCreate(data.name);

    Logger("[AttackEditor] New attack created\n");
}

void AttackDataEditor::DuplicateAttack()
{
    if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size())) return;

    AttackData copy  = attacks_->at(currentIndex_);
    copy.name       += "_copy";
    attacks_->push_back(copy);
    currentIndex_    = static_cast<int>(attacks_->size()) - 1;
    prevIndex_       = -1; // ★

    // ★ コピー元のフレームデータを複製する
    AttackFrameData& srcFd = AttackFrameDatabase::FindOrCreate(attacks_->at(currentIndex_ - 1).name);
    AttackFrameData  dstFd = srcFd;
    dstFd.attackName       = copy.name;
    AttackFrameDatabase::Get().push_back(dstFd);

    Logger("[AttackEditor] Attack duplicated\n");
}

void AttackDataEditor::DeleteAttack()
{
    if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size())) return;

    attacks_->erase(attacks_->begin() + currentIndex_);
    if (currentIndex_ >= static_cast<int>(attacks_->size()))
        currentIndex_ = static_cast<int>(attacks_->size()) - 1;

    prevIndex_ = -1; // ★
    dopeTracks_.clear();

    Logger("[AttackEditor] Attack deleted\n");
}

void AttackDataEditor::MoveUp()
{
    if (!attacks_ || currentIndex_ <= 0) return;
    std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ - 1));
    --currentIndex_;
}

void AttackDataEditor::MoveDown()
{
    if (!attacks_ || currentIndex_ < 0 || currentIndex_ + 1 >= static_cast<int>(attacks_->size())) return;
    std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ + 1));
    ++currentIndex_;
}

void AttackDataEditor::LoadFromJson()
{
    Logger("[AttackEditor] ===== Load Start =====\n");
    if (!attacks_) { Logger("[AttackEditor] ERROR: attacks_ is null!\n"); return; }

    bool loadResult = AttackDatabase::LoadFromFile(filePath_);
    Logger(loadResult ? "[AttackEditor] ===== Load Success =====\n"
                      : "[AttackEditor] ===== Load Failed =====\n");

    if (loadResult)
    {
        attacks_ = &AttackDatabase::Get();
        currentIndex_ = attacks_->empty() ? -1
            : std::clamp(currentIndex_, 0, static_cast<int>(attacks_->size()) - 1);
        prevIndex_ = -1; // ★ 再選択を強制
    }
}

void AttackDataEditor::SaveToJson()
{
    Logger("[AttackEditor] ===== Save Start =====\n");
    bool saveResult = AttackDatabase::SaveToFile(filePath_);
    Logger(saveResult ? "[AttackEditor] ===== Save Success =====\n"
                      : "[AttackEditor] ===== Save Failed =====\n");
}

void AttackDataEditor::TriggerReload()
{
    if (onReloadCallback_)
    {
        Logger("[AttackEditor] Triggering reload callback...\n");
        onReloadCallback_();
        Logger("[AttackEditor] Reload callback completed\n");
    }
}

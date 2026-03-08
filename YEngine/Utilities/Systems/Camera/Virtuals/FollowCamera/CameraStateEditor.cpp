#include "CameraStateEditor.h"
#include "FollowCamera.h"
#include "CinematicCameraState.h"
#include "ParryCameraState.h"
#include "BattleStartCameraState.h"
#include "DefaultCameraState.h"
#include "CameraStatePresetManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

CameraStateEditor* CameraStateEditor::GetInstance() {
    static CameraStateEditor instance;
    return &instance;
}

void CameraStateEditor::SetEditingState(std::unique_ptr<CameraState> state) {
    editingState_ = std::move(state);
}

void CameraStateEditor::DrawEditorWindow() {
#ifdef USE_IMGUI
    ImGui::Begin("Camera State Editor", nullptr, ImGuiWindowFlags_MenuBar);

    // ファイルパス（ウィンドウ上部に常時表示）
    static char filepath[256] = "camera_presets.json";

    // メニューバー
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("JSONに保存")) {
                CameraStatePresetManager::GetInstance()->SaveToFile(filepath);
                ImGui::OpenPopup("MenuFileSaved");
            }
            if (ImGui::MenuItem("JSONから読込")) {
                CameraStatePresetManager::GetInstance()->LoadFromFile(filepath);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 保存完了ポップアップ（メニューバー用）
    if (ImGui::BeginPopupModal("MenuFileSaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("'%s' に保存しました", filepath);
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ファイルパス入力を常時表示
    ImGui::InputText("JSONファイルパス", filepath, sizeof(filepath));
    ImGui::SameLine();
    if (ImGui::Button("保存##header")) {
        CameraStatePresetManager::GetInstance()->SaveToFile(filepath);
    }
    ImGui::SameLine();
    if (ImGui::Button("読込##header")) {
        CameraStatePresetManager::GetInstance()->LoadFromFile(filepath);
    }
    ImGui::Separator();

    // タブ
    if (ImGui::BeginTabBar("EditorTabs")) {
        if (ImGui::BeginTabItem("新規作成")) {
            DrawStateCreationUI();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("編集")) {
            DrawStateEditorUI();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("プリセット管理")) {
            DrawPresetManagerUI();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("プレビュー")) {
            DrawPreviewControlUI();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}

void CameraStateEditor::DrawStateCreationUI() {
#ifdef USE_IMGUI
    ImGui::Text("新しいカメラステートを作成");
    ImGui::Separator();

    const char* stateTypes[] = { "Cinematic", "Parry", "BattleStart" };
    ImGui::Combo("ステートタイプ", &selectedStateType_, stateTypes, 3);

    if (ImGui::Button("作成")) {
        switch (selectedStateType_) {
        case 0:
            editingState_ = std::make_unique<CinematicCameraState>();
            break;
        case 1:
            editingState_ = std::make_unique<ParryCameraState>();
            break;
        case 2:
            editingState_ = std::make_unique<BattleStartCameraState>();
            break;
        }
    }

    ImGui::Separator();

    if (editingState_) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "作成されたステート: %s", editingState_->GetStateName());
        ImGui::Text("「編集」タブで詳細を設定できます");
    }
#endif
}

void CameraStateEditor::DrawStateEditorUI() {
#ifdef USE_IMGUI
    if (!editingState_) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "編集するステートがありません");
        ImGui::Text("「新規作成」タブで作成するか、");
        ImGui::Text("「プリセット管理」タブから読み込んでください");
        return;
    }

    ImGui::Text("編集中のステート: %s", editingState_->GetStateName());
    ImGui::Separator();

    // ステート固有の編集UI
    editingState_->DrawEditGui();

    ImGui::Separator();

    // プリセット保存
    ImGui::Text("プリセットとして保存:");
    ImGui::InputText("プリセット名", newPresetName_, sizeof(newPresetName_));

    if (ImGui::Button("保存")) {
        if (strlen(newPresetName_) > 0) {
            CameraStatePresetManager::GetInstance()->SavePreset(
                newPresetName_,
                editingState_->GetStateName(),
                editingState_.get()
            );
            ImGui::OpenPopup("保存完了");
        }
    }

    // 保存完了ポップアップ
    if (ImGui::BeginPopupModal("保存完了", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("プリセットを保存しました: %s", newPresetName_);
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#endif
}

void CameraStateEditor::DrawPresetManagerUI() {
#ifdef USE_IMGUI
    ImGui::Text("プリセット一覧");
    ImGui::Separator();

    auto presetNames = CameraStatePresetManager::GetInstance()->GetPresetNames();

    if (presetNames.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "保存されたプリセットがありません");
        return;
    }

    for (const auto& name : presetNames) {
        ImGui::PushID(name.c_str());

        ImGui::Text("%s", name.c_str());
        ImGui::SameLine(200);

        if (ImGui::Button("編集")) {
            auto loadedState = CameraStatePresetManager::GetInstance()->LoadPreset(name);
            if (loadedState) {
                editingState_ = std::move(loadedState);
                strncpy_s(newPresetName_, name.c_str(), sizeof(newPresetName_) - 1);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("削除")) {
            ImGui::OpenPopup("削除確認");
        }

        // 削除確認ポップアップ
        if (ImGui::BeginPopupModal("削除確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("プリセット '%s' を削除しますか?", name.c_str());

            if (ImGui::Button("はい")) {
                CameraStatePresetManager::GetInstance()->DeletePreset(name);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("いいえ")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("カメラに適用")) {
            if (camera_) {
                auto loadedState = CameraStatePresetManager::GetInstance()->LoadPreset(name);
                if (loadedState) {
                    camera_->ChangeState(std::move(loadedState));
                }
            }
        }

        ImGui::PopID();
    }
#endif
}

void CameraStateEditor::DrawPreviewControlUI() {
#ifdef USE_IMGUI
    if (!camera_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "プレビュー用のカメラが設定されていません");
        return;
    }

    if (!editingState_) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "プレビューするステートがありません");
        return;
    }

    ImGui::Text("プレビューコントロール");
    ImGui::Separator();

    ImGui::Text("編集中のステート: %s", editingState_->GetStateName());
    ImGui::Spacing();

    // ── プレビュー開始 ──
    if (!isPreviewMode_) {
        if (ImGui::Button("▶  プレビュー開始")) {
            StartPreview();
        }
        ImGui::TextDisabled("※ プレビュー開始でカメラが演出を再生します");
    }
    else {
        // プレビュー中の操作
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "● 再生中");
        ImGui::SameLine();
        if (ImGui::Button("■  停止")) {
            StopPreview();
        }
        ImGui::SameLine();
        if (ImGui::Button("↺  最初から再生")) {
            StopPreview();
            StartPreview();
        }

        // 現在のステートが終了したら自動でフラグを戻す
        if (camera_->GetCurrentState() && camera_->GetCurrentState()->IsFinished()) {
            isPreviewMode_ = false;
        }
    }

    ImGui::Separator();

    // ── BattleStart 専用パネル ──
    if (std::string(editingState_->GetStateName()) == "BattleStart") {
        ImGui::Text("[ 戦闘開始カメラ専用 ]");

        if (ImGui::Button("ターゲット座標でプレビュー")) {
            // 制御点をターゲット基準で再構築してからプレビュー
            auto* bsState = static_cast<BattleStartCameraState*>(editingState_.get());
            bsState->SetupDefaultControlPoints(camera_);
            StopPreview();
            StartPreview();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(カメラのターゲット位置を使用)");
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Text("現在のカメラステート: %s",
        camera_->GetCurrentState() ? camera_->GetCurrentState()->GetStateName() : "なし");
#endif
}

// ── プライベートヘルパー実装 ──
void CameraStateEditor::StartPreview() {
    if (!camera_ || !editingState_) return;

    std::string stateName = editingState_->GetStateName();
    std::unique_ptr<CameraState> previewState;

    if (stateName == "BattleStart") {
        // BattleStart は全パラメータをメンバ変数で管理しているので
        // Save/Load で完全に複製できる
        auto dst = std::make_unique<BattleStartCameraState>();
        nlohmann::json j;
        editingState_->Save(j);
        dst->Load(j);
        // ターゲット座標で制御点を組み立て直す（位置だけ更新、回転・FOVは保持）
        dst->RebuildControlPoints(camera_);
        previewState = std::move(dst);

    }
    else if (stateName == "Parry") {
        auto dst = std::make_unique<ParryCameraState>();
        nlohmann::json j;
        editingState_->Save(j);
        dst->Load(j);
        previewState = std::move(dst);

    }
    else if (stateName == "Cinematic") {
        auto dst = std::make_unique<CinematicCameraState>();
        nlohmann::json j;
        editingState_->Save(j);
        dst->Load(j);
        previewState = std::move(dst);
    }

    if (previewState) {
        camera_->ChangeState(std::move(previewState));
        isPreviewMode_ = true;
    }
}

void CameraStateEditor::StopPreview() {
    if (!camera_) return;
    camera_->ChangeState(std::make_unique<DefaultCameraState>());
    isPreviewMode_ = false;
}
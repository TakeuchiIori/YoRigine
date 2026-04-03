#pragma once
// ===========================================================
// VfxMeshEditor.h
//
// ・ImGui でパラメータ編集
// ・TrailMesh / LightVolumeMesh を内部で持ちプレビューまで完結
// ・Undo/Redo は CommandHistory に委譲
// ===========================================================
#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <filesystem>
#include "VfxEffectAsset.h"
#include "TrailMesh.h"
#include "LightVolumeMesh.h"
#include "Editor/Command/CommandHistory.h"

namespace YoRigine {

    enum class VfxPreset : int
    {
        Blank = 0, TrailOnly, VolumeOnly, Sword, Magic, COUNT
    };

    class VfxMeshEditor
    {
    public:
        explicit VfxMeshEditor(const std::string& jsonPath = "");
        ~VfxMeshEditor() = default;

        // ----------------------------------------------------------
        // 毎フレーム
        // ----------------------------------------------------------

        /// ホットリロード監視 + プレビュー駆動
        /// プレビュー再生中は内部の TrailMesh / LightVolumeMesh を自動 Update する
        void Update(float deltaTime);

        /// ImGui ウィンドウ描画
        void DrawImGui();

        /// プレビュー描画 — ゲームの Draw パスと同じ場所で呼ぶ
        /// @param cmdList          コマンドリスト
        /// @param cameraGPUAddress gCamera CBV の GPU アドレス (他の Draw と同じ引数)
        void DrawPreview(ID3D12GraphicsCommandList* cmdList,
            D3D12_GPU_VIRTUAL_ADDRESS  cameraGPUAddress);

        // ----------------------------------------------------------
        // パラメータ参照
        // ----------------------------------------------------------
        const VfxEffectAsset& GetAsset() const { return asset_; }
        VfxEffectAsset& GetAsset() { return asset_; }

        // ----------------------------------------------------------
        // ファイル操作
        // ----------------------------------------------------------
        void Save(const std::string& path = "");
        bool Load(const std::string& path);
        void ForceReload() { if (!currentPath_.empty()) Load(currentPath_); }
        bool IsDirty() const { return isDirty_; }

        // ----------------------------------------------------------
        // コールバック
        // ----------------------------------------------------------
        using BrowseCallback = std::function<void(std::string& outPath)>;
        using OnChangedCallback = std::function<void(const VfxEffectAsset&)>;

        void SetBrowseCallback(BrowseCallback cb) { browseCallback_ = std::move(cb); }
        void SetOnChangedCallback(OnChangedCallback cb) { onChangedCallback_ = std::move(cb); }

    private:
        // ImGui サブセクション
        void DrawToolbar();
        void DrawFileSection();
        void DrawNewEffectDialog();
        void DrawNameSection();
        void DrawTrailSection();
        void DrawLightVolumeSection();
        void DrawPreviewSection();

        // パラメータ変更時に呼ぶ
        // before: 変更前の asset_ スナップショット (PushUndo の代わり)
        void CommitChange(const VfxEffectAsset& before, const char* label);

        void MarkDirty();

        // ホットリロード
        void PollFileChange();
        std::filesystem::file_time_type GetFileTime(const std::string& path) const;

        // プリセット
        static VfxEffectAsset MakePreset(VfxPreset preset);

        // メッシュをアセットパラメータで再初期化
        void RebuildMeshes();

        // ----------------------------------------------------------
        // 状態
        // ----------------------------------------------------------
        VfxEffectAsset asset_;
        std::string    currentPath_;
        bool           isDirty_ = false;
        bool           hotReload_ = true;
        bool           autoSave_ = false;

        // ホットリロード
        std::chrono::steady_clock::time_point lastCheckTime_;
        std::filesystem::file_time_type       lastFileTime_;
        float pollIntervalSec_ = 1.0f;

        // Undo/Redo — CommandHistory に完全委譲
        CommandHistory history_;

        // ----------------------------------------------------------
        // プレビュー用メッシュ (エディタが所有)
        // ----------------------------------------------------------
        std::unique_ptr<TrailMesh>       previewTrail_;
        std::unique_ptr<LightVolumeMesh> previewVolume_;

        // プレビュー再生状態
        bool  previewPlaying_ = false;
        float previewTimer_ = 0.f;

        // プレビュー用の仮トランスフォーム (ImGui で動かせる)
        Vector3 previewCenter_ = { 0.f, 0.f, 0.f };
        float   previewYaw_ = 0.f;

        // 新規作成ダイアログ
        bool showNewDialog_ = false;
        char newNameBuffer_[128] = "NewEffect";
        char newPathBuffer_[512] = "Resources/Vfx/NewEffect.json";
        int  newPresetIdx_ = static_cast<int>(VfxPreset::Sword);

        // デバッグ表示
        bool showTrailDebug_ = false;
        bool showVolumeDebug_ = false;

        // ImGui 用バッファ
        char nameBuffer_[128] = {};
        char textureBuffer_[512] = {};

        // コールバック
        BrowseCallback    browseCallback_;
        OnChangedCallback onChangedCallback_;
    };

} // namespace YoRigine
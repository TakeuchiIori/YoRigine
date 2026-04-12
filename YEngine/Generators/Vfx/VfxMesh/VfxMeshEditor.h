#pragma once
// ===========================================================
// VfxMeshEditor.h
// ===========================================================
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <wrl.h>
#include <d3d12.h>
#include "VfxEffectAsset.h"
#include "TrailMesh.h"
#include "LightVolumeMesh.h"
#include <Core/Editor/Command/CommandHistory.h>
#include "FileOperations/FileBrowser.h"  // YParticleEditor と同じパス

namespace YoRigine {

    class DirectXCommon;

    struct VfxEffectEntry
    {
        VfxEffectAsset asset;
        std::string    filePath;
        bool           isDirty = false;
    };

    struct MeshTrailParamsCB
    {
        float colorInner[4];
        float colorOuter[4];
        float softness;
        float glowPower;
        float distortion;
        float time;
    };

    struct LightVolumeParamsCB
    {
        float color[4];
        float edgeFade;
        float depthFade;
        float noiseTiling;
        float noiseStrength;
        float time;
        float _pad[3];
    };

    enum class VfxPreset : int
    {
        Blank = 0, TrailOnly, VolumeOnly, Sword, Magic, COUNT
    };

    enum class PreviewAnimMode : int {
        Wobble = 0,       // 従来の左右往復
        SlashHorizontal,  // 横なぎの剣閃
        SlashVertical,    // 縦斬りの剣閃
        Spin              // 回転斬り
    };

    class VfxMeshEditor
    {
    public:
        static VfxMeshEditor* GetInstance();

        void Initialize(const std::string& scanRoot = "Resources/Vfx/");
        void Finalize();

        void Update(float deltaTime);
        void DrawImGui();
        void DrawPreview(ID3D12GraphicsCommandList* cmdList,
            D3D12_GPU_VIRTUAL_ADDRESS  cameraGPUAddress);

    private:
        VfxMeshEditor();
        ~VfxMeshEditor() = default;

        // ImGui パネル
        void DrawListPanel();
        void DrawEditPanel();
        void DrawTrailSection();
        void DrawLightVolumeSection();
        void DrawPreviewSection();
        void DrawNewEffectDialog();

        // テクスチャ選択ポップアップ
        void DrawTextureSelectPopup();

        // エフェクト操作
        void ScanDirectory(const std::string& dir);
        void SelectEffect(int index);
        void SaveCurrent();
        void SaveAs(const std::string& newPath);
        void DeleteCurrent();
        void CreateNew(const std::string& name,
            const std::string& filePath,
            VfxPreset          preset);

        // Undo/Redo
        void CommitChange(const VfxEffectAsset& before, const char* label);

        // プレビュー用メッシュ / CBV
        void RebuildPreviewMeshes();
        void InitCBVs();
        void UpdateTrailCBV(float time);
        void UpdateVolumeCBV(float time);

        static VfxEffectAsset MakePreset(VfxPreset preset);

        // ----------------------------------------------------------
        // 状態
        // ----------------------------------------------------------
        DirectXCommon* dxCommon_ = nullptr;
        std::string    scanRoot_;

        std::vector<VfxEffectEntry> entries_;
        int selectedIndex_ = -1;

        VfxEffectEntry* Selected() {
            if (selectedIndex_ < 0 ||
                selectedIndex_ >= static_cast<int>(entries_.size())) return nullptr;
            return &entries_[selectedIndex_];
        }

        CommandHistory history_;

        // プレビュー
        std::unique_ptr<TrailMesh>       previewTrail_;
        std::unique_ptr<LightVolumeMesh> previewVolume_;

        PreviewAnimMode previewAnim_ = PreviewAnimMode::SlashHorizontal; // デフォルトを横なぎに
        float swordLength_ = 2.0f; // プレビュー用の剣の長さ

        bool    previewPlaying_ = false;
        float   previewTimer_ = 0.f;
        Vector3 previewCenter_ = { 0.f, 0.f, 0.f };
        float   previewYaw_ = 0.f;

        // CBV
        Microsoft::WRL::ComPtr<ID3D12Resource> trailCBResource_;
        MeshTrailParamsCB* trailCBMapped_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> volumeCBResource_;
        LightVolumeParamsCB* volumeCBMapped_ = nullptr;

        // 新規作成ダイアログ
        bool showNewDialog_ = false;
        char newNameBuffer_[128] = "NewEffect";
        char newPathBuffer_[512] = "";
        int  newPresetIdx_ = static_cast<int>(VfxPreset::Sword);

        // デバッグ
        bool showTrailDebug_ = false;
        bool showVolumeDebug_ = false;

        // ImGui 用バッファ
        char nameBuffer_[128] = {};

        // ----------------------------------------------------------
        // テクスチャ選択 FileBrowser
        // YParticleSystem と同じパターン:
        //   コンストラクタ本体で構築 → ThumbnailProvider → OnFileSelected
        // ----------------------------------------------------------
        FileBrowser rampBrowser_;           // ランプテクスチャ (t1: gTexRamp)
        bool        showRampPopup_ = false;

        FileBrowser noiseBrowser_;          // ノイズテクスチャ (t0: gTexNoise)
        bool        showNoisePopup_ = false;

    };

} // namespace YoRigine
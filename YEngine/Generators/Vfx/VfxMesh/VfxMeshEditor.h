#pragma once
// ===========================================================
// VfxMeshEditor.h
// ===========================================================
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <wrl.h>
#include <d3d12.h>
#include "VfxEffectAsset.h"
#include "LightVolumeMesh.h"
#include "VolumeSmokeMesh.h"
#include "LightningMesh.h"
#include "ShockwaveMesh.h"
#include <Core/Editor/Command/CommandHistory.h>
#include "FileOperations/FileBrowser.h"

// ★ TrailMesh ではなく Emitter を使う
#include "Vfx/VfxMesh/TrailMeshEmitter.h"
#include "Systems/Camera/Camera.h"

namespace YoRigine {

    class DirectXCommon;

    struct VfxEffectEntry
    {
        VfxEffectAsset asset;
        std::string    filePath;
        bool           isDirty = false;
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
        Wobble = 0,
        SlashHorizontal,
        SlashVertical,
        Spin
    };

    class VfxMeshEditor
    {
    public:
        static VfxMeshEditor* GetInstance();

        void Initialize(const std::string& scanRoot = "Resources/Vfx/");
        void Finalize();

        void Update(float deltaTime);
        void DrawImGui();

        // ★ 引数から cmdList を削除
        void DrawPreview();

        // ★ カメラをセットするための関数
        void SetCamera(Camera* camera) { camera_ = camera; }

    private:
        VfxMeshEditor();
        ~VfxMeshEditor() = default;

        void DrawListPanel();
        void DrawEditPanel();
        void DrawTrailSection();
        void DrawLightVolumeSection();
        void DrawSmokeSection();
        void DrawLightningSection();
        void DrawShockwaveSection();
        void DrawPreviewSection();
        void DrawNewEffectDialog();
        void DrawTextureSelectPopup();

        void ScanDirectory(const std::string& dir);
        void SelectEffect(int index);
        void SaveCurrent();
        void SaveAs(const std::string& newPath);
        void DeleteCurrent();
        void CreateNew(const std::string& name,
            const std::string& filePath,
            VfxPreset          preset);

        // 既存エフェクト名と衝突しない名前を返す。
        // base が空いていれば base、使われていれば base+"1", base+"2"... と空き番号を探す。
        std::string MakeUniqueEffectName(const std::string& base) const;

        void CommitChange(const VfxEffectAsset& before, const char* label);

        void RebuildPreviewMeshes();
        void InitCBVs();
        void UpdateVolumeCBV(float time);
        void UpdateSmokeCBV(float time);
        void UpdateLightningCBV(float time);
        void UpdateShockwaveCBV(float time);

        static VfxEffectAsset MakePreset(VfxPreset preset);

        DirectXCommon* dxCommon_ = nullptr;
        Camera* camera_ = nullptr; // ★追加
        std::string    scanRoot_;

        std::vector<VfxEffectEntry> entries_;
        int selectedIndex_ = -1;

        VfxEffectEntry* Selected() {
            if (selectedIndex_ < 0 ||
                selectedIndex_ >= static_cast<int>(entries_.size())) return nullptr;
            return &entries_[selectedIndex_];
        }

        CommandHistory history_;

        // ★ プレビュー（TrailはEmitterに完全委譲）
        std::unique_ptr<TrailMeshEmitter> previewTrailEmitter_;
        std::unique_ptr<LightVolumeMesh>  previewVolume_;
        std::unique_ptr<VolumeSmokeMesh>  previewSmoke_;
        std::unique_ptr<LightningMesh>    previewLightning_;
        std::unique_ptr<ShockwaveMesh>    previewShockwave_;

        PreviewAnimMode previewAnim_ = PreviewAnimMode::SlashHorizontal;
        float swordLength_ = 2.0f;

        bool    previewPlaying_ = false;
        float   previewTimer_ = 0.f;

        // 爆発ワンショット再生（破裂→膨張→消滅を1回。自動リピート）
        bool    oneShot_       = false;
        float   burstDuration_ = 2.0f; // 煙が漂う時間（爆発全体の長さ）
        float   burstProgress_ = -1.f; // -1=継続モード, 0..1=ワンショット進捗
        Vector3 smokeCenter_   = { 0.f, 0.f, 0.f }; // 上昇を反映した煙の中心
        float   smokeRadius_   = 1.5f;              // 膨張を反映した煙の半径（メッシュと一致）
        Vector3 previewCenter_ = { 0.f, 0.f, 0.f };
        float   previewYaw_ = 0.f;

        // ★ CBVは Volume のみ保持（TrailのCBVはEmitterが持っているため削除）
        Microsoft::WRL::ComPtr<ID3D12Resource> volumeCBResource_;
        LightVolumeParamsCB* volumeCBMapped_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> smokeCBResource_;
        SmokeParamsCB* smokeCBMapped_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> lightningCBResource_;
        LightningParamsCB* lightningCBMapped_ = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> shockwaveCBResource_;
        ShockwaveParamsCB* shockwaveCBMapped_ = nullptr;

        bool showNewDialog_ = false;
        char newNameBuffer_[128] = "NewEffect";
        char newPathBuffer_[512] = "";
        int  newPresetIdx_ = static_cast<int>(VfxPreset::Sword);

        bool showTrailDebug_ = false;
        bool showVolumeDebug_ = false;

        char nameBuffer_[128] = {};

        FileBrowser rampBrowser_;
        FileBrowser noiseBrowser_;
        bool        showRampPopup_ = false;
        bool        showNoisePopup_ = false;
    };

} // namespace YoRigine
#endif
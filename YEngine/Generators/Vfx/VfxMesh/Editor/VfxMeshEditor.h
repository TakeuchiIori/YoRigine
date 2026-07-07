#pragma once
// ===========================================================
// VfxMeshEditor.h
// ===========================================================
#ifdef USE_IMGUI
// C++
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <wrl.h>
#include <d3d12.h>

#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Effects/LightningMesh.h>
#include <Vfx/VfxMesh/Effects/LightVolumeMesh.h>
#include <Vfx/VfxMesh/Effects/VolumeSmokeMesh.h>
#include <Vfx/VfxMesh/Effects/ShockwaveMesh.h>
#include <Core/Editor/Command/CommandHistory.h>
#include "FileOperations/FileBrowser.h"

// TrailMesh ではなく Emitter を使う
#include "Vfx/VfxMesh/Runtime/TrailMeshEmitter.h"
#include "Systems/Camera/Camera.h"

namespace YoRigine {

    class DirectXCommon;

    struct VfxEffectEntry
    {
        VfxEffectAsset asset;
        std::string    filePath;
        bool           isDirty = false;
    };

    // LightVolumeParamsCB は LightVolumeMesh.h（YoRigine 名前空間）へ移設。
    // Editor / ランタイム Spawner の両方から使うため。

    enum class VfxPreset : int
    {
        Blank = 0, TrailOnly, VolumeOnly, WaypointBeam, Sword, Magic, Explosion, COUNT
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

        void Initialize(const std::string& scanRoot = "Resources/Json/VfxMesh/");
        void Finalize();

        void Update(float deltaTime);
        void DrawImGui();
        void DrawPreview();

        //カメラをセット
        void SetCamera(Camera* camera) { camera_ = camera; }

    private:
        VfxMeshEditor();
        ~VfxMeshEditor() = default;

        void DrawListPanel();
        void DrawEditPanel();
        void DrawTrailSection();
        // サブ効果（形状インスタンス）: 一覧＋追加/複製/削除
        void DrawSubEffectsSection();
        // 各種類のパラメータ編集（sub の対応パラメータを編集する）
        void DrawLightVolumeSection(VfxSubEffect& sub);
        void DrawSmokeSection(VfxSubEffect& sub);
        void DrawLightningSection(VfxSubEffect& sub);
        void DrawShockwaveSection(VfxSubEffect& sub);
        // モーションリスト編集（全体用と形状個別用で共用。showTarget=false で対象コンボを隠す）
        void DrawMotionListUI(std::vector<VfxMotion>& motions, bool showTarget, const char* commitLabel);
        void DrawMotionSection();
        static void ApplyDefaultSubEffectMotions(VfxSubEffect& sub);
        void DrawPreviewSection();
        void DrawNewEffectDialog();
        void DrawTextureSelectPopup();

        void ScanDirectory(const std::string& dir);
        void SelectEffect(int index);
        void SaveCurrent();
        void SaveAs(const std::string& newPath);
        // 現在選択中エフェクトの JSON ファイル名を newName に合わせてリネームする。
        // ディスク上の旧ファイルがあれば実際にリネームし、filePath も更新する。
        void RenameCurrentFile(const std::string& newName);
        void DeleteCurrent();
        void CreateNew(const std::string& name,
            const std::string& filePath,
            VfxPreset          preset);

        // 既存エフェクト名と衝突しない名前を返す。
        // base が空いていれば base、使われていれば base+"1", base+"2"... と空き番号を探す。
        std::string MakeUniqueEffectName(const std::string& base) const;

        void CommitChange(const VfxEffectAsset& before, const char* label);

        // アセットの subEffects と previewSubs_ の構成（数と種類）を同期させる。
        // 追加/削除/種類変更/Undo などで構成が変わったら作り直す。
        void SyncPreviewSubs();

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

        // サブ効果1個分のプレビューリソース（asset.subEffects と1対1対応）
        struct PreviewSub
        {
            VfxSubEffectType type = VfxSubEffectType::Smoke;

            // type に対応するものだけ生成される
            std::unique_ptr<LightVolumeMesh> volume;
            std::unique_ptr<VolumeSmokeMesh> smoke;
            std::unique_ptr<LightningMesh>   lightning;
            std::unique_ptr<ShockwaveMesh>   shockwave;

            // CB（実型は type で決まる）
            Microsoft::WRL::ComPtr<ID3D12Resource> cbRes;
            void*                                  cbMapped = nullptr;

            // Smoke の CB 用読み戻し値
            Vector3 smokeCenter = { 0.f, 0.f, 0.f };
            float   smokeRadius = 1.5f;

            // モーション評価結果（Update で書き込み、DrawPreview の CB 反映で使う）
            Vector4 tint            = { 1.f, 1.f, 1.f, 1.f }; // rgb=色乗算 / a=不透明度乗算
            float   beamRadiusScale = 1.f;
            float   beamGlowScale   = 1.f;
            bool    visible         = true;                   // Visibility モーションの結果

            ~PreviewSub() {
                if (cbMapped && cbRes) { cbRes->Unmap(0, nullptr); cbMapped = nullptr; }
            }
        };
        std::vector<std::unique_ptr<PreviewSub>> previewSubs_;

        PreviewAnimMode previewAnim_ = PreviewAnimMode::SlashHorizontal;
        float swordLength_ = 2.0f;    // プレビュー剣のサイズ（tip↔root）
        float previewSpeed_ = 1.0f;   // プレビュー振りの再生速度倍率
        bool  trailOneShot_ = false;  // トレイル: 1振りして消えるまで見せる（ワンショット）

        bool    previewPlaying_ = false;
        float   previewTimer_ = 0.f;

        // 汎用ワンショットループ（モーションの終端を自動検出してリピート。全エフェクト対応）
        bool    loopOneShot_        = false;
        float   loopDuration_       = 2.0f;  // モーションから自動検出できない場合のフォールバック秒数
        float   loopPeriodComputed_ = 0.0f;  // 直前フレームで算出した1サイクル長（UI表示用）

        // ワンショット再生（寿命＝モーション優先ぶんで1回発生 → 休止 → くりかえす）
        // 通常プレビューの既定挙動。loopOneShot_（モーション確認ループ）中のみ継続駆動になる。
        float   burstDuration_ = 2.0f; // 寿命フォールバック（BurstGrow モーションが無いとき）
        float   burstProgress_ = -1.f; // -1=継続(ループ確認中), 0..1=ワンショット進捗
        float   oneShotGap_    = 0.6f; // 一発出しきってから次の一発までの休止時間（完全に消える間）
        float   oneShotLocal_  = 0.f;  // 現サイクル内の経過秒（DrawPreview のサブ効果駆動に使う）
        bool    burstMode_     = false;// このフレームがワンショット駆動か（false=継続ループ確認）
        Vector3 previewCenter_ = { 0.f, 0.f, 0.f };
        float   previewYaw_ = 0.f;

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

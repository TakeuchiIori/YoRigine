#pragma once
// ===========================================================
// CompositeEffectManager.h
//
// 「複合エフェクト（Composite）」＝ Particle + VfxMesh + GPUParticle + Sound を
// 既存アセット名の参照だけで1つに束ねる薄いレイヤー。
//
// 既存3系統(Particle/VfxMesh/GPU)のJSON・ハンドルには一切手を入れず、
// それぞれの facade（EffectHandle / VfxMeshHandle / GpuParticleHandle）と
// Audio を「名前で呼ぶだけ」で連動させる。
//
// アセット: Resources/Json/YComposites/<名前>.json（参照リストのみ）
//   {
//     "name": "Explosion",
//     "particleEffect": "ExplosionSparks",
//     "vfxMeshAssets": [ { "asset": "ExplosionShockwave", "offset": [0,0,0], "scale": 1.5 } ],
//     "gpuEmitterGroup": "ExplosionDebris",
//     "sounds": [ { "path": "Resources/Audio/SE/explosion.wav", "volume": 1.0, "category": "SE" } ]
//   }
//
// ゲーム側: EffectHandle::PlayOneShot("Explosion", pos) の1行で4系統すべてが連動する
//           （EffectHandle の名前解決チェーン先頭に Composite を追加してある）。
// ===========================================================
#include "Vector3.h"
#include "Particle/EffectHandle.h"
#include "Vfx/VfxMesh/VfxMeshHandle.h"
#include "GPUParticle/GpuParticleHandle.h"
#include "Systems/Audio/Audio.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// ── アセット定義（既存アセット名の参照のみ）───────────────────────────
struct CompositeVfxRef {
    std::string asset;
    Vector3     offset = { 0.0f, 0.0f, 0.0f };
    float       scale = 1.0f;
};
struct CompositeSoundRef {
    std::string           path;
    float                 volume = 1.0f;
    YoRigine::SoundCategory category = YoRigine::SoundCategory::SE;
};
struct CompositeEffectAsset {
    std::string                    name;
    std::string                    particleEffect;    // 省略可
    std::vector<CompositeVfxRef>   vfxMeshAssets;     // 省略可
    std::string                    gpuEmitterGroup;   // 省略可
    std::vector<CompositeSoundRef> sounds;            // 省略可
};

// ── ループ複合エフェクトの実行インスタンス（子ハンドルを保持し Stop で連鎖停止）──
struct CompositeInstance {
    EffectHandle                        particle;   // Particle 子（ループ）
    std::vector<VfxMeshHandle>          vfx;        // VfxMesh 子（ループ）
    std::vector<Vector3>                vfxOffsets; // vfx と対の相対オフセット
    GpuParticleHandle                   gpu;        // GPU 子（ループ）
    std::vector<YoRigine::SoundHandle>  sounds;     // ループ音（保持して Stop 連鎖）
    Vector3                             basePos = { 0.0f, 0.0f, 0.0f };

    void SetPosition(const Vector3& pos);
    void Stop();
    bool IsActive() const;
};

// ── マネージャ（名前→アセット。描画はせず既存ハンドルを呼ぶだけの薄い層）──
class CompositeEffectManager {
public:
    static CompositeEffectManager* GetInstance();

    // YComposites/*.json を再帰スキャンして全ロード
    void ScanDirectory(const std::string& dir = "Resources/Json/YComposites/");
    bool LoadAsset(const std::string& filepath);

    bool Has(const std::string& name) const;

    // ワンショット（撃ちっぱなし。破片・爆発など）
    void PlayOneShot(const std::string& name, const Vector3& pos);
    // ループ（返り値の EffectHandle で追従・停止）
    EffectHandle Play(const std::string& name, const Vector3& pos);

    // 1件を Resources/Json/YComposites/<名前>.json に保存
    bool SaveAsset(const std::string& name);

#ifdef USE_IMGUI
    // 複合エフェクト編集UI（各系統の名前をドロップダウンで選ぶ＝手打ち撲滅）
    void DrawImGui();
#endif

private:
    CompositeEffectManager() = default;
    ~CompositeEffectManager() = default;
    CompositeEffectManager(const CompositeEffectManager&) = delete;
    CompositeEffectManager& operator=(const CompositeEffectManager&) = delete;

    std::unordered_map<std::string, CompositeEffectAsset> assets_;

#ifdef USE_IMGUI
    // 編集UI用の状態
    char             newCompositeName_[128] = "";
    std::string      selectedComposite_;
    Vector3          previewPos_ = { 0.0f, 2.0f, 0.0f };
    EffectHandle     previewLoopHandle_;          // ループ再生プレビューの保持
    std::vector<std::string> availableSounds_;    // Resources/Audio スキャン結果
    bool             soundsScanned_ = false;

    void ScanSounds();
#endif
};

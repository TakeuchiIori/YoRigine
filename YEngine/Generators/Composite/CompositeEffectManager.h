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
#include "Vfx/VfxMesh/Runtime/VfxMeshHandle.h"
#include "GPUParticle/GpuParticleHandle.h"
#include "Systems/Audio/Audio.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

// 一時オーバーラップクエリ（hitDelay到達時の QuerySphere）の結果型に使う。
// 重いヘッダ(CollisionManager.h)はここでは読み込まず、.cpp側でのみincludeする。
class BaseCollider;

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

    std::string                    particleEffect;      // 省略可
    Vector3                        particleOffset = { 0.0f, 0.0f, 0.0f }; // particleEffectの相対オフセット

    std::vector<CompositeVfxRef>   vfxMeshAssets;       // 省略可（offset/scaleは各要素が持つ）

    std::string                    gpuEmitterGroup;      // 省略可
    Vector3                        gpuOffset = { 0.0f, 0.0f, 0.0f };       // gpuEmitterGroupの相対オフセット

    std::vector<CompositeSoundRef> sounds;               // 省略可

    // ── ダメージ判定（一時オーバーラップクエリ） ──────────────────────
    // hitDelay<0 ならダメージクエリを発火しない。Play/PlayOneShotからこの秒数後に
    // CollisionManager::QuerySphere(pos, hitRadius, hitLayerMask) を1回だけ呼ぶ。
    // 見た目側の寿命（各子のJSON）とは独立した、ダメージ専用の時刻として扱う
    // （Docs/VfxExpansion_Design.md の 7.1 参照）。
    float    hitDelay     = -1.0f;
    float    hitRadius    = 1.5f;
    uint32_t hitLayerMask = 0xFFFFFFFFu;

    // ── カメラ演出フック ────────────────────────────────────────────
    // データ保持のみ。実際にCameraへ発火する配線は未実装（TODO。3.6参照）。
    std::string cameraShakeProfile;
    float       hitStopMs = 0.0f;

    // 全チャイルドの中で最大の自然な寿命（秒）。0以下=不明。
    //   - VfxMesh: VfxEffectAsset::OneShotDuration() から正確に計算
    //   - GPUParticle: GpuEmitManager::EstimateGroupNaturalDuration() から概算
    //   - CPUパーティクル(particleEffect): 現状未対応（System=定義/インスタンス=粒バッファが
    //     未分離なため安全に見積もれない）。0扱い＝このCompositeのNaturalDurationに寄与しない。
    float NaturalDuration() const;
};

// ── ループ複合エフェクトの実行インスタンス（子ハンドルを保持し Stop で連鎖停止）──
struct CompositeInstance {
    EffectHandle                        particle;       // Particle 子（ループ）
    Vector3                             particleOffset = { 0.0f, 0.0f, 0.0f };
    std::vector<VfxMeshHandle>          vfx;            // VfxMesh 子（ループ）
    std::vector<Vector3>                vfxOffsets;     // vfx と対の相対オフセット
    GpuParticleHandle                   gpu;            // GPU 子（ループ）
    Vector3                             gpuOffset = { 0.0f, 0.0f, 0.0f };
    std::vector<YoRigine::SoundHandle>  sounds;         // ループ音（保持して Stop 連鎖）
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

    // 毎フレーム呼ぶこと（hitDelayの遅延ダメージクエリを消化する）。
    // VfxMeshSpawner::Update等と同じ場所（GameScene/DevelopSceneのUpdate）で呼ぶ想定。
    void Update(float deltaTime);

    // Play/PlayOneShot の拡張パラメータ。省略時は全て既定値＝従来と同じ挙動。
    struct PlayParams {
        // このComposite全体の見た目寿命がminDuration秒を下回らないよう、
        // 対応可能な子（現状VfxMeshのみ。7.2参照）を自動で引き伸ばす。0=指定なし。
        // 例: atk.totalFrames/atk.fps をそのまま渡す。
        float minDuration = 0.0f;

        // hitDelay到達時に呼ばれる（QuerySphereの結果を渡す）。
        // asset側のhitDelay<0なら呼ばれない。ダメージ適用はコールバック側の責務
        // （CompositeEffectManagerはHP/ダメージAPIを知らない設計を維持する）。
        std::function<void(const std::vector<BaseCollider*>&)> onHitQuery;
    };

    // ワンショット（撃ちっぱなし。破片・爆発など）
    void PlayOneShot(const std::string& name, const Vector3& pos);
    void PlayOneShot(const std::string& name, const Vector3& pos, const PlayParams& params);
    // ループ（返り値の EffectHandle で追従・停止）
    EffectHandle Play(const std::string& name, const Vector3& pos);
    EffectHandle Play(const std::string& name, const Vector3& pos, const PlayParams& params);

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

    // ── hitDelay用の遅延ダメージクエリ待ち行列（Update()で毎フレーム消化） ──
    struct PendingHitQuery {
        Vector3  pos;
        float    radius = 1.5f;
        uint32_t layerMask = 0xFFFFFFFFu;
        float    remaining = 0.0f;
        std::function<void(const std::vector<BaseCollider*>&)> callback;
    };
    std::vector<PendingHitQuery> pendingHitQueries_;

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

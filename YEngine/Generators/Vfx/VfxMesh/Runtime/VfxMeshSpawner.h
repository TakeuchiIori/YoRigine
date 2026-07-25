#pragma once
// ===========================================================
// VfxMeshSpawner.h
//
// VfxMesh 系エレメント（LightningBolt / ShockwaveRing / NoiseVolume など）を
// 名前とトランスフォームで生成・管理するシングルトン。
// アセットは JSON から事前ロードしてプールで管理するため、
// Spawn 時のヒープ確保は発生しない。
//
// 使い方（MyGame 側）:
//   // 初期化
//   VfxMeshSpawner::GetInstance()->Initialize();
//   VfxMeshSpawner::GetInstance()->ScanDirectory("Resources/Json/VfxMesh/");
//
//   // 毎フレーム
//   VfxMeshSpawner::GetInstance()->SetCamera(camera);
//   VfxMeshSpawner::GetInstance()->Update(dt);
//   VfxMeshSpawner::GetInstance()->Draw();
//
// ゲームコードからは YVfxHandle 経由で操作するほうが簡潔。
// ===========================================================
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Core/ProceduralMeshBase.h>
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>
#include "Vector3.h"
#include "Memory/PoolAllocator.h"

class Camera;

namespace YoRigine { class DirectXCommon; }

class VfxMeshSpawner
{
public:
    static VfxMeshSpawner* GetInstance();

    // ── ライフサイクル ────────────────────────────────────────

    /// Registry への型登録と内部リソースの初期化を行う
    void Initialize();

    /// アクティブなエフェクトを全解放する（アセットマップも含む。アプリ終了時用）
    void Finalize();

    /// アクティブなエフェクトのみを全停止する（アセットは保持する。シーン遷移時用）
    void StopAll();

    /// 描画に使うカメラをセットする（毎フレーム呼ぶ）
    void SetCamera(Camera* camera) { camera_ = camera; }

    // ── アセット登録 ─────────────────────────────────────────

    /// JSON ファイル 1 つをロードして assetMap_ に登録する
    void LoadAsset(const std::string& filePath);

    /// ディレクトリ以下の全 .json をスキャンしてロードする
    void ScanDirectory(const std::string& dir = "Resources/Json/VfxMesh/");

    /// ロード済みアセット名の一覧を返す（エディタのドロップダウン用）
    std::vector<std::string> GetAssetNames() const;

    /// ロード済みアセットを名前で取得する。未登録なら nullptr
    const YoRigine::VfxEffectAsset* GetAsset(const std::string& assetName) const;

    // ── 毎フレーム ────────────────────────────────────────────

    /// アクティブなエフェクトを全て更新し、寿命が来たものを解放する
    void Update(float deltaTime);

    /// アクティブなエフェクトを全て描画する
    void Draw();

    // ── 生成 ─────────────────────────────────────────────────

    /// 位置 + スケール指定で生成する（スモーク・衝撃波など球系向け）
    /// timeScale: age の進み方の倍率。<1 で寿命を引き伸ばす
    /// lifetime : 寿命(秒)の明示指定。>0 ならアセットの OneShotDuration を無視して
    ///            この秒数で寿命(バースト進捗 0→1)が完結し破棄される。
    ///            <=0 (既定) ならアセット既定の寿命を使う。
    ///            loop / loopForever より優先され、指定時は必ずこの秒数で消える。
    /// 戻り値: エフェクト ID（0 は失敗 / プール満杯）
    uint32_t Spawn(const std::string& assetName,
                   const Vector3&     position,
                   float              scale     = 1.0f,
                   bool               loop      = false,
                   float              timeScale = 1.0f,
                   float              lifetime  = -1.0f);

    /// 始点・終点指定で生成する（稲妻など方向性エフェクト向け）
    uint32_t SpawnBolt(const std::string& assetName,
                       const Vector3&     start,
                       const Vector3&     end,
                       bool               loop      = false,
                       float              timeScale = 1.0f);

    // ── 操作（YVfxHandle から呼ばれる） ──────────────────────

    /// ID で指定したエフェクトの位置を更新する
    void SetPosition(uint32_t id, const Vector3& pos);

    /// ID で指定したエフェクトのスケールを更新する
    void SetScale(uint32_t id, float scale);

    /// ID で指定した方向性エフェクトの端点を更新する
    void SetEndpoints(uint32_t id, const Vector3& start, const Vector3& end);

    /// ID で指定したエフェクトの timeScale を変更する
    void SetTimeScale(uint32_t id, float timeScale);

    /// ID で指定したエフェクトのインスタンス色（tint）を設定する。
    /// アセット色に乗算される（rgb=色乗算 / a=不透明度乗算）。
    /// 同じアセットを種類別に色替えして量産する用途（バフ/デバフ等）。
    void SetColor(uint32_t id, const Vector4& color);

    /// ID で指定したエフェクトの全エレメントに種別インデックスを流し込む。
    /// 各マテリアルが自分の使う引数だけ拾う（AreaField=(a:魔法陣, b:内部質感) / RimFx=(c:縁)）。
    /// 同一アセットを種類別に見た目替えして量産する用途（バフ/デバフ等）。
    void SetStyleIndices(uint32_t id, int a, int b, int c);

    /// ID で指定したエフェクトを即座に停止・破棄する
    void Stop(uint32_t id);

    /// ID で指定したエフェクトがまだ生存しているか
    bool IsAlive(uint32_t id) const;

private:
    VfxMeshSpawner()  = default;
    ~VfxMeshSpawner() = default;
    VfxMeshSpawner(const VfxMeshSpawner&)            = delete;
    VfxMeshSpawner& operator=(const VfxMeshSpawner&) = delete;

    /// 同時に存在できる VfxMesh エフェクトの上限（PoolAllocator の固定長）
    static constexpr size_t kMaxActiveVfx = 256;

    // ── エレメント1個分の実行時リソース ──────────────────────
    // VfxEffectAsset::elements と 1 対 1 対応
    struct ElementRT
    {
        const YoRigine::VfxElement*                   def  = nullptr; ///< アセット内の定義（参照のみ）
        std::unique_ptr<YoRigine::ProceduralMeshBase>  mesh;          ///< 実際のメッシュ実体

        Microsoft::WRL::ComPtr<ID3D12Resource> cbRes;    ///< インスタンス独立の定数バッファ
        void*                                  cbMapped = nullptr;

        // モジュール評価結果（UpdateEffect で書き込み、DrawEffect の FillCB で使う）
        Vector4 tint            = { 1.f, 1.f, 1.f, 1.f };
        float   beamRadiusScale = 1.f;
        float   beamGlowScale   = 1.f;
        bool    visible         = true;
    };

    // ── アクティブエフェクト ──────────────────────────────────
    struct ActiveEffect
    {
        uint32_t id        = 0;
        bool     alive     = true;
        bool     loop      = false;
        float    age       = 0.f;
        float    timeScale = 1.0f; ///< age の進み方の倍率（<1 で寿命を引き伸ばす）
        float    lifetimeOverride = -1.f; ///< >0 で寿命(秒)を明示指定（loop より優先）。<=0 でアセット既定

        /// アセットはコピーせず assetMap_ の実体を指す（生成時のコピーを回避）
        const YoRigine::VfxEffectAsset* asset = nullptr;

        Vector3 position  = { 0.f, 0.f, 0.f };
        float   scale     = 1.0f;
        Vector3 boltStart = { 0.f, 0.f, 0.f };
        Vector3 boltEnd   = { 0.f, 3.f, 0.f };

        /// false: LightningBolt はアセットの direction/length から端点を自動計算する
        /// true : SetEndpoints / SpawnBolt で明示指定された端点をそのまま使う
        bool    explicitEndpoints = false;

        float burstProgress = -1.f; ///< -1=ループ継続 / 0..1=ワンショット進捗

        /// インスタンス色（tint）。アセット色＋モジュール評価結果に更に乗算される。
        Vector4 userColor = { 1.f, 1.f, 1.f, 1.f };

        std::vector<ElementRT> subs; ///< エレメントごとの実行時リソース

        /// CB の Unmap とメッシュの破棄を行う
        void Release();

        ~ActiveEffect() { Release(); }
    };

    /// アセットの elements に従って ElementRT を生成し fx.subs に詰める
    void InitEffect(ActiveEffect& fx);

    /// モジュール評価 → Drive → Update の順に各エレメントを更新する
    void UpdateEffect(ActiveEffect& fx, float dt);

    /// fx の全エレメントを PSO バインド → FillCB → Draw で描画する
    void DrawEffect(ActiveEffect& fx);

    YoRigine::DirectXCommon* dxCommon_ = nullptr;
    Camera*  camera_  = nullptr;
    uint32_t nextId_  = 1; ///< 次に発行する ID（単調増加）

    std::unordered_map<std::string, YoRigine::VfxEffectAsset> assetMap_;

    /// 実体は固定長プールから確保（生成/破棄でヒープが増減しない）
    PoolAllocator<ActiveEffect, kMaxActiveVfx> pool_;
    /// 今生きているエフェクトへのポインタ列（reserve 済みで再確保しない）
    std::vector<ActiveEffect*> active_;
};

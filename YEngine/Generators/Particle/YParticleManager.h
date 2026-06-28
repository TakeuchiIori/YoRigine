#pragma once

#include "YParticleSystem.h"
#include "YParticleRenderer.h"
#include "Systems/Camera/Camera.h"
#include "DirectX/SrvManager.h"
#include "YParticleStructs.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// YParticleSystem を統合管理するマネージャー
/// システムの作成、更新、描画を一元管理
/// </summary>
class YParticleManager {
public:
    //=================================================================
    // シングルトン
    //=================================================================

    static YParticleManager& GetInstance() {
        static YParticleManager instance;
        return instance;
    }

    YParticleManager(const YParticleManager&) = delete;
    YParticleManager& operator=(const YParticleManager&) = delete;

    //=================================================================
    // 初期化・終了
    //=================================================================

    /// <summary>
    /// マネージャーの初期化
    /// </summary>
    /// <param name="srvManager">SRVマネージャー</param>
    /// <param name="maxTotalParticles">全システム合計の最大パーティクル数</param>
    void Initialize(SrvManager* srvManager, uint32_t maxTotalParticles = 10000);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    //=================================================================
    // システム管理
    //=================================================================

    /// <summary>
    /// パーティクルシステムの作成
    /// </summary>
    /// <param name="name">システム名（一意）</param>
    /// <param name="maxParticles">このシステムの最大パーティクル数</param>
    /// <returns>作成されたシステムのポインタ</returns>
    YParticleSystem* CreateSystem(const std::string& name, uint32_t maxParticles = 1000);

    /// <summary>
    /// システムの取得
    /// </summary>
    /// <param name="name">システム名</param>
    /// <returns>システムのポインタ（存在しない場合nullptr）</returns>
    YParticleSystem* GetSystem(const std::string& name);

    /// <summary>
    /// システムの削除
    /// </summary>
    /// <param name="name">システム名</param>
    void RemoveSystem(const std::string& name);

    /// <summary>
    /// システムの名前を変更
    /// </summary>
    /// <param name="oldName">現在の名前</param>
    /// <param name="newName">新しい名前</param>
    /// <returns>成功したらtrue（新しい名前が既存の場合false）</returns>
    bool RenameSystem(const std::string& oldName, const std::string& newName);

    /// <summary>
    /// すべてのシステム名を取得
    /// </summary>
    std::vector<std::string> GetAllSystemNames() const;

    /// <summary>
    /// システム数を取得
    /// </summary>
    size_t GetSystemCount() const { return systems_.size(); }

    //=================================================================
    // ファイル操作（追加）
    //=================================================================

    /// <summary>
    /// JSONファイルからすべてのシステムを読み込み
    /// </summary>
    bool LoadSystemsFromFile(const std::string& filePath);

    /// <summary>
    /// JSONからすべてのシステムを読み込み
    /// </summary>
    void LoadSystemsFromJson(const nlohmann::json& json);

    /// <summary>
    /// すべてのシステムをファイルに保存
    /// </summary>
    bool SaveSystemsToFile(const std::string& filePath) const;

    /// <summary>
    /// システムとグループを1つのバンドルJSONからまとめてロード
    /// フォーマット: {"systems":[...], "groups":[...]}
    /// </summary>
    bool LoadEffectBundle(const std::string& filePath);

    /// <summary>
    /// 指定グループと参照システムをバンドルJSONとして保存
    /// </summary>
    bool SaveEffectBundle(const std::string& groupName, const std::string& filePath);

    //=================================================================
    // 更新・描画
    //=================================================================

    /// <summary>
    /// すべてのシステムを更新
    /// </summary>
    /// <param name="deltaTime">経過時間</param>
    void Update(float deltaTime);

    /// <summary>
    /// すべてのシステムを描画
    /// テクスチャごとにバッチングして描画
    /// </summary>
    void Draw();

    //=================================================================
    // 便利メソッド（エミッター用）
    //=================================================================

    /// <summary>
    /// 指定システムからパーティクルを発生
    /// </summary>
    void Emit(const std::string& systemName, const Vector3& position, int count = 1);

    /// <summary>
    /// 指定システムからバースト発生
    /// </summary>
    void EmitBurst(const std::string& systemName, const Vector3& position, int count);

    //=================================================================
    // カメラ設定
    //=================================================================

    void SetCamera(Camera* camera) { camera_ = camera; }
    Camera* GetCamera() const { return camera_; }

    //=================================================================
    // 統計情報
    //=================================================================

    struct Statistics {
        size_t totalSystems = 0;
        size_t activeSystems = 0;
        size_t totalActiveParticles = 0;
        float updateTimeMs = 0.0f;
        float renderTimeMs = 0.0f;
    };

    const Statistics& GetStatistics() const { return stats_; }


private:
    YParticleManager() = default;
    ~YParticleManager() { Finalize(); }

    //=================================================================
    // 描画の内部処理
    //=================================================================

    /// <summary>
    /// テクスチャ＋メッシュの組み合わせごとにバッチング
    /// </summary>
    struct RenderBatch {
        std::shared_ptr<Mesh> mesh;
        uint32_t textureIndex;
        ParticleLightSetting lightSetting;
        std::vector<YParticleSystem*> systems;
		BlendMode blendMode;
    };

    void CreateRenderBatches(std::vector<RenderBatch>& batches);

private:
    //=================================================================
    // メンバ変数
    //=================================================================

    bool initialized_ = false;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    // パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // システム管理
    std::unordered_map<std::string, std::unique_ptr<YParticleSystem>> systems_;

    // レンダラー
    std::unique_ptr<YParticleRenderer> renderer_;

    // 統計情報
    Statistics stats_;
};
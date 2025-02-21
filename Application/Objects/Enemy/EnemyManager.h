#pragma once

#include <memory>
#include <vector>
#include "Enemy.h"
#include "Systems/Camera/Camera.h"
#include "Player/Player.h"
#include <Systems/GameTime/HitStop.h>

class EnemyManager {
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(Camera* camera);

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 敵のスポーン処理
    /// </summary>
    void SpawnEnemy();

    // プレイヤーの設定
    void SetPlayer(Player* player) { player_ = player; }
    Enemy* GetEnemy(int i) { return enemies_[i].get(); }
    size_t GetEnemyCount() const { return enemies_.size(); }
     bool IsAllEnemiesDefeated() const { return deadNum_ >= static_cast<int>(maxEnemyCount_); }
    
     // 全ての敵にヒットストップを適用
     void ApplyHitStopToAllEnemies(HitStop::HitStopType type) {
         for (auto& enemy : enemies_) {
             // 各敵のtimeIDを取得してヒットストップを適用
             std::string timeID = "Enemy : " + std::to_string(enemy->GetSerialNumber());
             HitStop::GetInstance()->Start(timeID, type);
         }
     }

private:
    // 各種パラメータ
    float spawnTimer_ = 0.0f;
    float spawnInterval_ = 5.0f;  // スポーン間隔（秒）
    float spawnRange_ = 30.0f;    // スポーン範囲
    const uint32_t maxEnemyCount_ = 5;  // 最大敵数
    int deadNum_ = 0;

    // ポインタ
    std::vector<std::unique_ptr<Enemy>> enemies_;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
};

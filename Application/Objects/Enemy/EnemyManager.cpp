#include "EnemyManager.h"
#include <random>

void EnemyManager::Initialize(Camera* camera) {
    camera_ = camera;
    deadNum_ = 0;
}

void EnemyManager::Update() {
    // スポーンタイマーの更新
    spawnTimer_ += 1.0f / 60.0f;
    if (spawnTimer_ >= spawnInterval_ && enemies_.size() < maxEnemyCount_) {
        SpawnEnemy();
        spawnTimer_ = 0.0f;
    }

    // 敵の更新
    for (auto it = enemies_.begin(); it != enemies_.end();) {
        auto& enemy = *it;
        enemy->Update();

        // 敵が死んでいたら削除
        if (!enemy->IsActive()) {
            deadNum_++;
            it = enemies_.erase(it);  // eraseは次の有効なイテレータを返す
        }
        else {
            ++it;
        }
    }
}

void EnemyManager::Draw() {
    for (auto& enemy : enemies_) {
        enemy->Draw();
    }
}

void EnemyManager::DrawCollision()
{
	for (auto& enemy : enemies_) {
		enemy->DrawCollision();
	}
}

void EnemyManager::SpawnEnemy() {
    if (!player_) return;

    // 乱数生成
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-spawnRange_, spawnRange_);

    // プレイヤーの位置を基準にランダムな位置を計算
    Vector3 playerPos = player_->GetWorldPosition();
    Vector3 spawnPos = {
        playerPos.x + dist(gen),
        1.0f,  // Y座標は固定
        playerPos.z + dist(gen)
    };

    // 新しい敵を生成
    auto newEnemy = std::make_unique<Enemy>();
    newEnemy->Initialize(camera_, { -30.0f, 0.0f, 20.0f });
	newEnemy->SetPosition(spawnPos);
    newEnemy->SetEnemyManager(this);
    newEnemy->SetPlayer(player_);

    // リストに追加
    enemies_.push_back(std::move(newEnemy));
}
#pragma once
#include "Sprite/Sprite.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Loaders/Json/JsonManager.h"
#include <memory>

// 前方宣言
class BattleEnemy;
class Camera;

class EnemyHealthBarUI {
public:
    // コンストラクタ
    EnemyHealthBarUI(const BattleEnemy* enemy, Camera* camera);

    // 初期化 (UIBase::Initializeをオーバーロード)
    void Initialize();

    // 更新処理 (ビルボード処理と表示ポリシーの適用)
    void Update();

	// 描画処理
    void Draw();
private:
    void InitJson();
private:
    const BattleEnemy* targetEnemy_ = nullptr; // 追従対象の敵
    Camera* camera_ = nullptr;                 // カメラ

    // HPバーの背景とゲージ用
    std::unique_ptr<Sprite> bgHP_;
    std::unique_ptr<Sprite> barHP_;

    // HPの割合
    float currentRatio_ = 1.0f;

    // ビルボード（3D空間）の位置
    Vector3 worldPosition_;

	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
    Vector3 offset_{};
    Vector2 size_{ 252.400f ,38.900f };
	Vector2 anchorPoint_{ 0.0f, 0.5f };
    Vector4 color{1.0f,0.0f,0.0f,1.0f};
    float kReferenceDistance_ = 20.0f;
};
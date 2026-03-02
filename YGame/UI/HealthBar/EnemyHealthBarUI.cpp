#include "EnemyHealthBarUI.h"
#include "Enemy/BattleEnemy/BattleEnemy.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/UI/UIManager.h>

EnemyHealthBarUI::EnemyHealthBarUI(const BattleEnemy* enemy, Camera* camera)
{
	targetEnemy_ = enemy;
	camera_ = camera;
}

void EnemyHealthBarUI::Initialize()
{
	bgHP_ = std::make_unique<Sprite>();
	barHP_ = std::make_unique<Sprite>();


	bgHP_->Initialize("Resources/Textures/GameScene/EnemyHPBg.png");
	barHP_->Initialize("Resources/Textures/GameScene/EnemyHPBar.png");
	barHP_->SetColor(color);

	// 背景は「中央」基準（敵の座標にそのまま置くため）
	bgHP_->SetAnchorPoint({ 0.5f, 0.5f });
	// バーは「左」基準（右側を削る動きにするため）
	barHP_->SetAnchorPoint({ 0.0f, 0.5f });

	InitJson();
}

void EnemyHealthBarUI::Update()
{
    // 座標変換を行って depth (w) を取得する
    Vector3 worldPos = targetEnemy_->GetTranslate();
    worldPos += offset_;

    Vector4 clipSpacePos = Transform({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, camera_->GetViewProjectionMatrix());

    // カメラの後ろにいる場合は処理しない（w <= 0 は描画範囲外）
    if (clipSpacePos.w <= 0.0f) {
        // 必要なら非表示にする処理を入れる
        return;
    }

    // スケール計算（距離に反比例させる）
    float distanceScale = kReferenceDistance_ / clipSpacePos.w;

    // 近すぎるときに巨大になりすぎないように制限（最大2倍など）
    distanceScale = std::min(distanceScale, 2.0f);
    // ------------------------------------------

    // 透視除算
    clipSpacePos.x /= clipSpacePos.w;
    clipSpacePos.y /= clipSpacePos.w;

    // スクリーン座標変換
    Vector3 screenPos;
    screenPos.x = (clipSpacePos.x * 0.5f + 0.5f) * WinApp::kClientWidth;
    screenPos.y = (-clipSpacePos.y * 0.5f + 0.5f) * WinApp::kClientHeight;
    screenPos.z = 0.0f;

    // HP割合計算
    float ratio = (float)targetEnemy_->GetCurrentHP() / (float)targetEnemy_->GetMaxHP();
    ratio = std::clamp(ratio, 0.0f, 1.0f);

    // --- サイズ設定にスケールを適用 ---
    // 元のサイズ(size_)に、距離スケール(distanceScale)を掛ける
    Vector2 scaledSize = size_ * distanceScale;

    bgHP_->SetSize(scaledSize);
    barHP_->SetSize({ scaledSize.x * ratio, scaledSize.y });

    // --- 座標適用 ---
    bgHP_->SetTranslate(screenPos);

    // バーの位置調整も、スケール後のサイズで行う必要がある
    // (scaledSize を使うのがポイント)
    Vector3 barOffset = Vector3(-scaledSize.x * (0.5f - anchorPoint_.x), 0.0f, 0.0f);
    barHP_->SetTranslate(screenPos + barOffset);

    bgHP_->Update();
    barHP_->Update();
}



void EnemyHealthBarUI::Draw()
{
	if (YoRigine::GameTime::IsPause()) {
        return;
    }
	bgHP_->Draw();
	barHP_->Draw();
}

void EnemyHealthBarUI::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("EnemyHealthBarUI", "Resources/Json/UI/");
	jsonManager_->SetCategory("UI");
	jsonManager_->SetSubCategory("EnemyHealthBarUI");
	jsonManager_->Register("サイズ", &size_);
	jsonManager_->Register("オフセット", &offset_);
	jsonManager_->Register("基準距離", &kReferenceDistance_);
	jsonManager_->Register("色", &color);
}

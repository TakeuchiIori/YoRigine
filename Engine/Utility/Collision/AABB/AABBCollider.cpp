#include "AABBCollider.h"

void AABBCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("BaseCollider Offset Min X", &aabbOffset_.min.x);
	jsonManager->Register("BaseCollider Offset Min Y", &aabbOffset_.min.y);
	jsonManager->Register("BaseCollider Offset Min Z", &aabbOffset_.min.z);
	jsonManager->Register("BaseCollider Offset Max X", &aabbOffset_.max.x);
	jsonManager->Register("BaseCollider Offset Max Y", &aabbOffset_.max.y);
	jsonManager->Register("BaseCollider Offset Max Z", &aabbOffset_.max.z);
	
}

void AABBCollider::Initialize()
{
	BaseCollider::Initialize();

	aabb_.min = { 0.0f,0.0f,0.0f };
	aabb_.max = { 0.0f,0.0f,0.0f };

	aabbOffset_.min = { -1.0f,-1.0f,-1.0f };
	aabbOffset_.max = { 1.0f,1.0f,1.0f };
}

void AABBCollider::Update()
{
	aabb_.min = GetCenterPosition() + aabbOffset_.min;
	aabb_.max = GetCenterPosition() + aabbOffset_.max;
}

void AABBCollider::Draw()
{
	line_->DrawAABB(aabb_.min, aabb_.max);
	line_->DrawLine();
}

#include "AABBCollider.h"

void AABBCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("Collider Offset Min X", &aabbOffset_.min.x);
	jsonManager->Register("Collider Offset Min Y", &aabbOffset_.min.y);
	jsonManager->Register("Collider Offset Min Z", &aabbOffset_.min.z);
	jsonManager->Register("Collider Offset Max X", &aabbOffset_.max.x);
	jsonManager->Register("Collider Offset Max Y", &aabbOffset_.max.y);
	jsonManager->Register("Collider Offset Max Z", &aabbOffset_.max.z);
	
}

Vector3 AABBCollider::GetCenterPosition() const
{
	Vector3 newPos;
	newPos.x = wt_->matWorld_.m[3][0];
	newPos.y = wt_->matWorld_.m[3][1];
	newPos.z = wt_->matWorld_.m[3][2];
	return newPos;
}

const WorldTransform& AABBCollider::GetWorldTransform()
{
	return *wt_;
}

Vector3 AABBCollider::GetEulerRotation() const
{
	return wt_ ? wt_->rotation_ : Vector3{};
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
	Vector3 scale = GetWorldTransform().scale_;;
	Vector3 center = GetCenterPosition();

	Vector3 size = {
		(aabbOffset_.max.x - aabbOffset_.min.x) * scale.x,
		(aabbOffset_.max.y - aabbOffset_.min.y) * scale.y,
		(aabbOffset_.max.z - aabbOffset_.min.z) * scale.z,
	};

	Vector3 halfSize = {
		size.x * 0.5f,
		size.y * 0.5f,
		size.z * 0.5f,
	};

	Vector3 min = center - halfSize;
	Vector3 max = center + halfSize;

	// min/maxを正規化（負スケールでも対応）
	aabb_.min = {
		(std::min)(min.x, max.x),
		(std::min)(min.y, max.y),
		(std::min)(min.z, max.z),
	};
	aabb_.max = {
		(std::max)(min.x, max.x),
		(std::max)(min.y, max.y),
		(std::max)(min.z, max.z),
	};
}

void AABBCollider::Draw()
{
	line_->DrawAABB(aabb_.min, aabb_.max);
	line_->DrawLine();
}

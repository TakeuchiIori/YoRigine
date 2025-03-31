#include "AABBCollider.h"


AABBCollider::~AABBCollider()
{
	BaseCollider::~BaseCollider();

}

void AABBCollider::InitJson(JsonManager* jsonManager)
{
	// 衝突球のオフセットや半径を JSON に登録
	jsonManager->SetCategory("Colliders");
	jsonManager->Register("AABB Offset Min X", &aabbOffset_.min.x);
	jsonManager->Register("AABB Offset Min Y", &aabbOffset_.min.y);
	jsonManager->Register("AABB Offset Min Z", &aabbOffset_.min.z);
	jsonManager->Register("AABB Offset Max X", &aabbOffset_.max.x);
	jsonManager->Register("AABB Offset Max Y", &aabbOffset_.max.y);
	jsonManager->Register("AABB Offset Max Z", &aabbOffset_.max.z);
	
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
	Vector3 scale = GetScale();
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

Vector3 AABBCollider::GetScale() const
{
	return worldTransform_->scale_;
}

Vector3 AABBCollider::GetAnchorPoint() const
{
	return worldTransform_->anchorPoint_;
}

Vector3 AABBCollider::GetCenterPosition() const {

	Vector3 newPos;
	newPos.x = worldTransform_->matWorld_.m[3][0];
	newPos.y = worldTransform_->matWorld_.m[3][1];
	newPos.z = worldTransform_->matWorld_.m[3][2];
	return newPos;
}

Matrix4x4 AABBCollider::GetWorldMatrix() const {
	return worldTransform_ ? worldTransform_->matWorld_ : MakeIdentity4x4();
}

Vector3 AABBCollider::GetEulerRotation() const {
	return worldTransform_ ? worldTransform_->rotation_ : Vector3{};
}

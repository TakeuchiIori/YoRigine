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
	aabb_.min = GetCenterPosition() + aabbOffset_.min;
	aabb_.max = GetCenterPosition() + aabbOffset_.max;
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

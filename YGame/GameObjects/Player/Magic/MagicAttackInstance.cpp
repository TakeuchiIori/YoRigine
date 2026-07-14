#include "MagicAttackInstance.h"

#include "GameObjects/Enemy/BattleEnemy/BattleEnemy.h"
#include "GameObjects/Player/Camera/PlayerCamera.h"
#include "GameObjects/Player/Player.h"
#include "UI/Damage/DamageNumberManager.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/Core/CollisionTypeIdDef.h"

#include <algorithm>
#include <cmath>

void MagicAttackInstance::Initialize(const MagicActionData& action, Player* owner)
{
	action_ = action;
	owner_ = owner;
	elapsedTime_ = 0.0f;
	hitColliders_.clear();

	wt_.Initialize();
	if (owner_) {
		origin_ = ResolveOrigin(*owner_);
		target_ = ResolveTarget(*owner_, origin_);
		wt_.translate_ = origin_;
	}
	wt_.UpdateMatrix();

	collider_ = ColliderFactory::Create<SphereCollider>(
		this,
		&wt_,
		owner_ && owner_->GetPlayerCamera() ? owner_->GetPlayerCamera()->GetLastCamera() : nullptr,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayerMagic));

	if (collider_) {
		collider_->SetIsStatic(false);
		collider_->SetMass(0.0f);
		collider_->SetEnablePenetration(false);
		collider_->SetCheckOutsideCamera(false);
		collider_->SetCollisionMask(CollisionLayerBit(CollisionLayer::Enemy));
		collider_->SetRadius(std::max(0.01f, action_.hitRadius * action_.scaleCurve.Evaluate(0.0f)));
	}

	alive_ = true;
}

void MagicAttackInstance::Update(float deltaTime)
{
	if (!alive_) return;

	elapsedTime_ += deltaTime;
	const float duration = std::max(0.01f, action_.duration);
	const float t = std::clamp(elapsedTime_ / duration, 0.0f, 1.0f);

	switch (action_.trajectoryType) {
	case MagicTrajectoryType::StrikeTarget:
		wt_.translate_ = target_;
		break;
	case MagicTrajectoryType::Forward:
	case MagicTrajectoryType::LockOnOrForward:
	default:
		wt_.translate_ = origin_ + (target_ - origin_) * t;
		break;
	}

	wt_.UpdateMatrix();
	if (collider_) {
		const bool active = elapsedTime_ >= action_.hitDelay;
		collider_->SetCollisionEnabled(active);
		collider_->SetRadius(std::max(0.01f, action_.hitRadius * action_.scaleCurve.Evaluate(t)));
		collider_->Update();
	}

	if (elapsedTime_ >= duration) {
		alive_ = false;
		if (collider_) collider_->SetCollisionEnabled(false);
	}
}

void MagicAttackInstance::DrawCollision()
{
	if (collider_ && alive_) collider_->Draw();
}

void MagicAttackInstance::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other)
{
	ApplyHit(other);
}

void MagicAttackInstance::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}
void MagicAttackInstance::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}
void MagicAttackInstance::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}
void MagicAttackInstance::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

Vector3 MagicAttackInstance::ResolveOrigin(Player& owner) const
{
	return owner.GetWorldPosition() + Vector3{ 0.0f, 1.25f, 0.0f } + ResolveForward(owner) * 1.2f;
}

Vector3 MagicAttackInstance::ResolveForward(Player& owner) const
{
	const float yaw = owner.GetCameraRotation().y;
	Vector3 forward{ std::sin(yaw), 0.0f, std::cos(yaw) };
	forward = Vector3::Normalize(forward);
	if (std::abs(forward.x) < 0.0001f && std::abs(forward.z) < 0.0001f) {
		return Vector3{ 0.0f, 0.0f, 1.0f };
	}
	return forward;
}

Vector3 MagicAttackInstance::ResolveTarget(Player& owner, const Vector3& origin) const
{
	if (action_.trajectoryType != MagicTrajectoryType::Forward) {
		if (PlayerCamera* camera = owner.GetPlayerCamera()) {
			if (BaseCollider* target = camera->GetLockedTarget()) {
				return target->GetCenterPosition();
			}
		}
	}

	return origin + ResolveForward(owner) * std::max(0.0f, action_.range);
}

void MagicAttackInstance::ApplyHit(BaseCollider* other)
{
	if (!other) return;
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) return;
	if (hitColliders_.contains(other)) return;
	hitColliders_.insert(other);

	BattleEnemy* enemy = other->GetOwnerAs<BattleEnemy>();
	if (!enemy || !enemy->IsAlive()) return;

	float power = action_.hitRadius;
	for (const auto& event : action_.events) {
		power = std::max(power, event.power);
	}
	const int damage = static_cast<int>(std::max(0.0f, power));
	enemy->TakeDamage(damage);
	DamageNumberManager::GetInstance()->SpawnDamage(damage, enemy->GetTranslate(), false);
}

#include "MagicVfxEventHandler.h"

#include "GameObjects/Player/Camera/PlayerCamera.h"
#include "GameObjects/Player/Player.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshHandle.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"
#include "Collision/Core/BaseCollider.h"

#include <algorithm>
#include <cmath>

void MagicVfxEventHandler::Execute(const MagicTimelineEvent& event, const MagicEventContext& context)
{
	if (!context.owner) return;

	Player& player = *context.owner;
	const Vector3 origin = ResolveCastOrigin(player);
	const Vector3 target = ResolveTargetPoint(player, event, origin);
	const float scale = ResolveScale(event, context.chargeTime);
	const float timeScale = ResolveTimeScale(event);

	switch (event.type) {
	case MagicEventType::PlayVfx:
		VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Smoke"), origin, std::max(scale * 0.35f, 0.2f), timeScale);
		break;
	case MagicEventType::SpawnBeam:
		VfxMeshHandle::PlayBolt(ResolveAssetName(event, "Lightning"), origin, target, false, timeScale);
		break;
	case MagicEventType::SpawnProjectile:
		// まだ弾オブジェクトを所有しない段階では、入力データの「発射」を
		// 起点→着弾点の軌跡VFXと着弾VFXへ変換する。弾寿命・追尾・判定は次の層で足す。
		VfxMeshHandle::PlayBolt("Lightning", origin, target, false, timeScale);
		VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Explosion"), target, scale, timeScale);
		break;
	case MagicEventType::SpawnArea:
		VfxMeshHandle::PlayOneShot(ResolveAssetName(event, "Explosion"), target, std::max(event.radius, scale), timeScale);
		break;
	case MagicEventType::StrikeTarget:
		VfxMeshHandle::PlayBolt(ResolveAssetName(event, "Lightning"), target + Vector3{ 0.0f, 12.0f, 0.0f }, target, false, timeScale);
		VfxMeshHandle::PlayOneShot("Explosion", target, scale, timeScale);
		break;
	default:
		break;
	}
}

Vector3 MagicVfxEventHandler::ResolveCastOrigin(Player& player) const
{
	return player.GetWorldPosition() + Vector3{ 0.0f, 1.25f, 0.0f } + ResolveForward(player) * 1.2f;
}

Vector3 MagicVfxEventHandler::ResolveForward(Player& player) const
{
	const float yaw = player.GetCameraRotation().y;
	Vector3 forward{ std::sin(yaw), 0.0f, std::cos(yaw) };
	forward = Vector3::Normalize(forward);
	if (std::abs(forward.x) < 0.0001f && std::abs(forward.z) < 0.0001f) {
		return Vector3{ 0.0f, 0.0f, 1.0f };
	}
	return forward;
}

Vector3 MagicVfxEventHandler::ResolveTargetPoint(Player& player, const MagicTimelineEvent& event, const Vector3& origin) const
{
	if (PlayerCamera* camera = player.GetPlayerCamera()) {
		if (BaseCollider* target = camera->GetLockedTarget()) {
			return target->GetCenterPosition();
		}
	}

	return origin + ResolveForward(player) * ResolveRange(event, 0.0f);
}

float MagicVfxEventHandler::ResolveRange(const MagicTimelineEvent& event, float chargeTime) const
{
	const float dataRange = event.speed > 0.0f ? event.speed : 18.0f;
	const float chargeBonus = std::clamp(chargeTime, 0.0f, 2.0f) * 4.0f;
	return dataRange + chargeBonus;
}

float MagicVfxEventHandler::ResolveScale(const MagicTimelineEvent& event, float chargeTime) const
{
	const float baseScale = event.radius > 0.0f ? event.radius : std::max(event.power * 0.15f, 1.0f);
	return baseScale + std::clamp(chargeTime, 0.0f, 2.0f) * 0.35f;
}

float MagicVfxEventHandler::ResolveTimeScale(const MagicTimelineEvent& event) const
{
	if (event.duration <= 0.0f) return 1.0f;

	// 現行の Lightning は約0.25秒のワンショットなので、データ側の duration を
	// 「見せたい尺」として扱い、アセット時間を伸縮する。厳密な寿命管理はVFX層に閉じる。
	return std::clamp(0.25f / event.duration, 0.1f, 4.0f);
}

std::string MagicVfxEventHandler::ResolveAssetName(const MagicTimelineEvent& event, const std::string& fallback) const
{
	if (!event.label.empty() && VfxMeshSpawner::GetInstance()->GetAsset(event.label)) {
		return event.label;
	}
	return fallback;
}

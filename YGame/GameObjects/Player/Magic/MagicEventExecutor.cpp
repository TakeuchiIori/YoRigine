#include "MagicEventExecutor.h"

MagicEventExecutor::MagicEventExecutor()
{
	// まずは全イベントをDebug実行へ流す。
	// 本実装時は Beam / Projectile / Strike などを個別Handlerへ差し替える。
	handlers_[MagicEventType::Debug] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::PlayVfx] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::SpawnBeam] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::SpawnProjectile] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::SpawnArea] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::StrikeTarget] = std::make_unique<DebugMagicEventHandler>();
}

void MagicEventExecutor::Execute(const MagicTimelineEvent& event, const MagicEventContext& context)
{
	auto it = handlers_.find(event.type);
	if (it == handlers_.end() || !it->second) return;
	it->second->Execute(event, context);
}

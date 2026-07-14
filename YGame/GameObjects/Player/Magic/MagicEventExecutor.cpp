#include "MagicEventExecutor.h"

MagicEventExecutor::MagicEventExecutor()
{
	// Debug はログ専用に残し、ゲームに見える魔法イベントは VFX 実行層へ渡す。
	// Runner は「いつ発火するか」だけを担当し、何を出すかは Handler 側へ分けて肥大化を防ぐ。
	handlers_[MagicEventType::Debug] = std::make_unique<DebugMagicEventHandler>();
	handlers_[MagicEventType::PlayVfx] = std::make_unique<MagicVfxEventHandler>();
	handlers_[MagicEventType::SpawnBeam] = std::make_unique<MagicVfxEventHandler>();
	handlers_[MagicEventType::SpawnProjectile] = std::make_unique<MagicVfxEventHandler>();
	handlers_[MagicEventType::SpawnArea] = std::make_unique<MagicVfxEventHandler>();
	handlers_[MagicEventType::StrikeTarget] = std::make_unique<MagicVfxEventHandler>();
}

void MagicEventExecutor::Execute(const MagicTimelineEvent& event, const MagicEventContext& context)
{
	auto it = handlers_.find(event.type);
	if (it == handlers_.end() || !it->second) return;
	it->second->Execute(event, context);
}

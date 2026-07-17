#pragma once

#include "DebugMagicEventHandler.h"
#include "MagicVfxEventHandler.h"

#include <memory>
#include <unordered_map>

class MagicEventExecutor {
public:
	MagicEventExecutor();
	void Execute(const MagicTimelineEvent& event, const MagicEventContext& context);

private:
	std::unordered_map<MagicEventType, std::unique_ptr<IMagicEventHandler>> handlers_;
};

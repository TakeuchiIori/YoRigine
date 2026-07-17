#pragma once

#include "IMagicEventHandler.h"

class DebugMagicEventHandler : public IMagicEventHandler {
public:
	void Execute(const MagicTimelineEvent& event, const MagicEventContext& context) override;
};

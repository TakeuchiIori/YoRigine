#pragma once

#include "MagicActionData.h"
#include "MagicEventContext.h"

class IMagicEventHandler {
public:
	virtual ~IMagicEventHandler() = default;
	virtual void Execute(const MagicTimelineEvent& event, const MagicEventContext& context) = 0;
};

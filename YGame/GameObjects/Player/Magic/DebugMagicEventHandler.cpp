#include "DebugMagicEventHandler.h"

#include <Debugger/Logger.h>

void DebugMagicEventHandler::Execute(const MagicTimelineEvent& event, const MagicEventContext& context)
{
	const std::string message =
		"[MagicEvent] type=" + std::string(ToString(event.type)) +
		", element=" + ToString(event.element) +
		", label=" + event.label +
		", elapsed=" + std::to_string(context.elapsedTime) +
		", charge=" + std::to_string(context.chargeTime) + "\n";
	Logger(message.c_str());
}

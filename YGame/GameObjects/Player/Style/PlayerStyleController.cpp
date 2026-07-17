#include "PlayerStyleController.h"

void PlayerStyleController::Reset()
{
	currentStyle_ = PlayerStyle::Sword;
}

void PlayerStyleController::Toggle()
{
	currentStyle_ = (currentStyle_ == PlayerStyle::Sword)
		? PlayerStyle::Magic
		: PlayerStyle::Sword;
}

void PlayerStyleController::SetStyle(PlayerStyle style)
{
	currentStyle_ = style;
}

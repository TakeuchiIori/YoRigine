#pragma once

class Player;

struct MagicEventContext {
	Player* owner = nullptr;
	float elapsedTime = 0.0f;
	float chargeTime = 0.0f;
};

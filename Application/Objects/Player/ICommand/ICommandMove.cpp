#include "ICommandMove.h"
#include "Player/Player.h"


void MoveFrontCommand::Exec(Player& player)
{
	player.MoveFront();
}

void MoveBehindCommand::Exec(Player& player)
{
	player.MoveBehind();
}

void MoveRightCommand::Exec(Player& player)
{
	player.MoveRight();
}

void MoveLeftCommand::Exec(Player& player)
{
	player.MoveLeft();
}

ICommandMove::~ICommandMove()
{
}

#pragma once
#include "Player/ICommand/ICommandMove.h"


class InputHandleMove
{
public:
	ICommandMove* HandleInput();

	void AssignMoveFrontCommandPressKeyW();
	void AssignMoveBehindCommandPressKeyS();
	void AssignMoveRightCommandPressKeyD();
	void AssignMoveLeftCommandPressKeyA();

private:
	ICommandMove* pressKeyW_;
	ICommandMove* pressKeyS_;
	ICommandMove* pressKeyD_;
	ICommandMove* pressKeyA_;
};


#include "InputHandleMove.h"
#include "Systems/Input/Input.h"

ICommandMove* InputHandleMove::HandleInput()
{
	if (Input::GetInstance()->PushKey(DIK_W)) {
		return pressKeyW_;
	}
	else if (Input::GetInstance()->PushKey(DIK_S)) {
		return pressKeyS_;
	}
	else if (Input::GetInstance()->PushKey(DIK_D)) {
		return pressKeyD_;
	}
	else if (Input::GetInstance()->PushKey(DIK_A)) {
		return pressKeyA_;
	}
	return nullptr;
}


void InputHandleMove::AssignMoveFrontCommandPressKeyW()
{
	ICommandMove* command = new MoveFrontCommand();
	this->pressKeyW_ = command;
}

void InputHandleMove::AssignMoveBehindCommandPressKeyS()
{
	ICommandMove* command = new MoveBehindCommand();
	this->pressKeyS_ = command;
}

void InputHandleMove::AssignMoveRightCommandPressKeyD()
{
	ICommandMove* command = new MoveRightCommand();
	this->pressKeyD_ = command;
}

void InputHandleMove::AssignMoveLeftCommandPressKeyA()
{
	ICommandMove* command = new MoveLeftCommand();
	this->pressKeyA_ = command;
}

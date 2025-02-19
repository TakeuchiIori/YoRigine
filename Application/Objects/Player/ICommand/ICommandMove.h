#pragma once
class Player;
class ICommandMove
{
public:
	virtual ~ICommandMove();
	virtual void Exec(Player& Player) = 0;
};


class MoveFrontCommand : public ICommandMove {
public:
	void Exec(Player& player) override;

};

class MoveBehindCommand : public ICommandMove {
public:
	void Exec(Player& player) override;

};

class MoveRightCommand : public ICommandMove {
public:
	void Exec(Player& player) override;

};

class MoveLeftCommand : public ICommandMove {
public:
	void Exec(Player& player) override;

};

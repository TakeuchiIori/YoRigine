#pragma once
class ISceneTransition
{
public:
	virtual ~ISceneTransition() = default;
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual bool IsFinished() const = 0;
	virtual void StartTransition() = 0;
	virtual void EndTransition() = 0;
};


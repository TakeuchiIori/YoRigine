#pragma once

#include "Sprite/SpriteCommon.h"
#include "../Base/ISceneTransition.h"
#include "Fade.h"
class FadeTransition : public ISceneTransition {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    bool IsFinished() const override;
    void StartTransition()override;
    void EndTransition()override;

private:
    std::unique_ptr<Fade> fade_;
	float FADE_DURATION = 2.0f;

};


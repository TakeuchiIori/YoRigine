#pragma once
#include <memory>
#include "ISceneTransition.h"
#include <MainScenes/Transitions/Fade/FadeTransition.h>

// TransitionFactory.h
class TransitionFactory {
public:
    virtual ~TransitionFactory() = default;
    virtual std::unique_ptr<ISceneTransition> CreateTransition() = 0;
};

// FadeTransitionFactory.h
class FadeTransitionFactory : public TransitionFactory {
public:
    std::unique_ptr<ISceneTransition> CreateTransition() override {
        return std::make_unique<FadeTransition>();
    }
};



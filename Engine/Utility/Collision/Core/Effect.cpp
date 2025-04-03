#include "Effect.h"
// Engine
#include "Loaders./Model/ModelManager.h"

// C++
#include "MathFunc.h"
#include <cmath> // sin, cos 用
#include <Particle/ParticleManager.h>

void Effect::Initialize() {
    worldTransform_.Initialize();
    obj_ = std::make_unique<Object3d>();
    obj_->Initialize();
    obj_->SetModel("ball.obj");
    rotationAngle_ = 0.0f; // 回転角度初期化
    vibrationAmount_ = 0.0f; // 振動量初期化
}

void Effect::Update() {
    // 時間に基づく補間
    float t = (10.0f - timer_) * 0.1f;
    float scale = Lerp(1.0f, 3.0f, t);

    // スケールの動き
    worldTransform_.scale_ = { scale * 2, scale * 2, scale * 2 };

    // 回転の追加
    rotationAngle_ += 5.0f; // 回転速度
    if (rotationAngle_ > 360.0f) {
        rotationAngle_ -= 360.0f;
    }
    worldTransform_.rotation_ = { 0.0f, rotationAngle_, 0.0f };

    // 振動の追加
    vibrationAmount_ = sin(timer_ * 0.5f) * 0.5f; // 振動幅
    worldTransform_.translation_ = { vibrationAmount_, vibrationAmount_, 0.0f };

    // 透明度の変更（コメントアウト部分を復元）
    float alpha = 1.0f - Lerp(1.0f, 0.0f, timer_ / 10.0f);
    obj_->SetAlpha(alpha);

    // 行列の更新
    worldTransform_.UpdateMatrix();

    //ParticleManager::GetInstance()->Emit("W", worldTransform_.translation_, 10);

    // タイマーの減少と終了判定
    timer_--;
    if (timer_ < 0) {
        isAlive_ = false;
    }
}

void Effect::Draw(Camera* camera) {
    obj_->Draw(camera,worldTransform_);
}

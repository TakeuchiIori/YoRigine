#define NOMINMAX
#include "DebugCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <Systems/Input/Input.h>
#include <DirectXMath.h>
#include <algorithm>

#ifdef _DEBUG
#include <imgui.h>
#endif

void DebugCamera::Initialize()
{
    InitJson();

    // カメラの初期位置を設定
    translate_ = { 0.0f, 6.0f, -40.0f };

    // 初期のマウス位置を取得
    Input* input = Input::GetInstance();
    prevMousePos_ = input->GetMousePosition();
}

void DebugCamera::Update()
{
    UpdateInput();

    // ビュー行列を作成
    matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));

#ifdef _DEBUG
    // ImGuiの描画処理（コメントアウト中）
#endif // _DEBUG
}

void DebugCamera::UpdateInput()
{
    Input* input = Input::GetInstance();

    // ===== マウスによる操作 =====
    Vector2 currentMousePos = input->GetMousePosition();
    
    // 右クリックで回転
    if (input->IsPressMouse(1)) {
        if (!isDragging_) {
            isDragging_ = true;
            prevMousePos_ = currentMousePos;
        }

        // マウスの移動量を計算
        float deltaX = currentMousePos.x - prevMousePos_.x;
        float deltaY = currentMousePos.y - prevMousePos_.y;

        // マウスの移動量に応じて回転を更新
        rotate_.y += deltaX * rotateSpeed_ * 0.01f;
        rotate_.x += deltaY * rotateSpeed_ * 0.01f;

        // 上下回転が一定範囲を超えないように制限
        rotate_.x = std::max(-1.5f, std::min(1.5f, rotate_.x));
    }  
    else if (input->IsPressMouse(0)) {
        if (!isDragging_) {
            isDragging_ = true;
            prevMousePos_ = currentMousePos;
        }

        // マウスの移動量を計算
        float deltaX = currentMousePos.x - prevMousePos_.x;
        float deltaY = currentMousePos.y - prevMousePos_.y;

        // 移動量に応じてカメラの位置を変更
        Vector3 moveDelta = { -deltaX * moveSpeed_ * 0.1f, deltaY * moveSpeed_ * 0.1f, 0.0f };

        // カメラの回転に応じた移動ベクトルを計算
        Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
        Vector3 transformedMove = TransformNormal(moveDelta, rotMat);

        // 移動を適用
        translate_ += transformedMove;
    } else {
        isDragging_ = false;
    }


    // 現在のマウス位置を保存
    prevMousePos_ = currentMousePos;

    // マウスホイールで前後移動
    int32_t wheel = input->GetWheel();
    if (wheel != 0) {
        Vector3 forward = TransformNormal({ 0, 0, 1.0f }, MakeRotateMatrixXYZ(rotate_));
        translate_ += forward * wheel * moveSpeed_ * 0.1f;
    }

    // ===== キーボードによる操作 =====
    // WASDで水平移動、QEで上下移動
    Vector3 moveDirection = { 0, 0, 0 };

    if (input->PushKey(DIK_W)) moveDirection.z += 1.0f;
    if (input->PushKey(DIK_S)) moveDirection.z -= 1.0f;
    if (input->PushKey(DIK_A)) moveDirection.x -= 1.0f;
    if (input->PushKey(DIK_D)) moveDirection.x += 1.0f;
    if (input->PushKey(DIK_Q) || input->IsPressMouse(3)) moveDirection.y += 1.0f;
    if (input->PushKey(DIK_E) || input->IsPressMouse(4)) moveDirection.y -= 1.0f;

    // Shiftキーを押すと移動速度が上がる
    float speedMultiplier = 1.0f;
    if (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT)) {
        speedMultiplier = 3.0f;
    }

    if (Length(moveDirection) > 0.0f) {
        // 移動方向を正規化
        moveDirection = Normalize(moveDirection);

        // カメラの回転に応じた移動ベクトルを計算
        Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
        Vector3 transformedMove = TransformNormal(moveDirection, rotMat);

        // 移動を適用
        translate_ += transformedMove * moveSpeed_ * speedMultiplier;
    }

    // ===== コントローラーによる操作 =====
    if (input->IsControllerConnected()) {
        XINPUT_STATE joyState;
        if (input->GetJoystickState(0, joyState)) {
            // 右スティックで回転
            Vector3 rotateMove{};
            rotateMove.y += static_cast<float>(joyState.Gamepad.sThumbRX) * rotateSpeedController_ * 0.0001f;
            rotateMove.x -= static_cast<float>(joyState.Gamepad.sThumbRY) * rotateSpeedController_ * 0.0001f;

            rotate_ += rotateMove;

            // 上下回転が一定範囲を超えないように制限
            rotate_.x = std::max(-1.5f, std::min(1.5f, rotate_.x));

            // 左スティックで移動
            Vector3 stickMove{};
            stickMove.x = static_cast<float>(joyState.Gamepad.sThumbLX);
            stickMove.z = static_cast<float>(joyState.Gamepad.sThumbLY);

            // デッドゾーン処理を行い、移動を正規化
            if (Length(stickMove) > 0.0f) {
                stickMove = Normalize(stickMove) * moveSpeedController_ * 0.1f;

                // カメラの回転に応じた移動ベクトルを計算
                Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
                Vector3 transformedMove = TransformNormal(stickMove, rotMat);

                // 移動を適用
                translate_ += transformedMove;
            }

            // トリガーボタンで上下移動
            if (joyState.Gamepad.bLeftTrigger > 0) {
                translate_.y -= moveSpeedController_ * 0.1f * (joyState.Gamepad.bLeftTrigger / 255.0f);
            }
            if (joyState.Gamepad.bRightTrigger > 0) {
                translate_.y += moveSpeedController_ * 0.1f * (joyState.Gamepad.bRightTrigger / 255.0f);
            }
        }
    }
}

void DebugCamera::InitJson()
{
    // JSON管理クラスを初期化
    jsonManager_ = std::make_unique<JsonManager>("DebugCamera", "Resources/JSON");
    jsonManager_->Register("Translate", &translate_);
    jsonManager_->Register("Rotate", &rotate_);
    jsonManager_->Register("RotateSpeed", &rotateSpeed_);
    jsonManager_->Register("RotateSpeed Controller", &rotateSpeedController_);
    jsonManager_->Register("MoveSpeed", &moveSpeed_);
    jsonManager_->Register("MoveSpeed Controller", &moveSpeedController_);
}

void DebugCamera::JsonImGui()
{
    // JSONのImGui表示処理（コメントアウト中）
}

void DebugCamera::ImGui()
{
#ifdef _DEBUG
    ImGui::Begin("DebugCamera Info");

    // ImGuiでカメラの位置や回転を調整
    ImGui::DragFloat3("Position", &translate_.x, 0.1f);
    ImGui::DragFloat3("Rotation", &rotate_.x, 0.01f);
    ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.01f, 2.0f);
    ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.1f, 10.0f);

    ImGui::End();
#endif // _DEBUG
}

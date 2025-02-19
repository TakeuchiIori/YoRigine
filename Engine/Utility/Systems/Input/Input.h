#pragma once
// C++
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION     0x0800
#include "dinput.h"
#include "assert.h"
#include <array>
#include <vector>
#include <Xinput.h>
#include <chrono>

// Engine
#include "WinApp./WinApp.h"

// Math
#include "Vector2.h"
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"xinput.lib")

template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;


enum class GamePadButton {
    A = XINPUT_GAMEPAD_A,
    B = XINPUT_GAMEPAD_B,
    X = XINPUT_GAMEPAD_X,
    Y = XINPUT_GAMEPAD_Y,
    LB = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RB = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    Start = XINPUT_GAMEPAD_START,
    Back = XINPUT_GAMEPAD_BACK,
    LT = XINPUT_GAMEPAD_LEFT_THUMB,
    RT = XINPUT_GAMEPAD_RIGHT_THUMB,
};

/// <summary>
/// 入力
/// </summary>
class Input
{
public: // インナークラス
    struct MouseMove {
        LONG lX;
        LONG lY;
        LONG lZ;
    };
    enum class PadType {
        DirectInput,
        XInput,
    };

    // variantがC++17から
    union State {
        XINPUT_STATE xInput_;
        DIJOYSTATE2 directInput_;
    };
    struct Joystick {
        Microsoft::WRL::ComPtr<IDirectInputDevice8> device_;
        int32_t deadZoneL_;
        int32_t deadZoneR_;
        PadType type_;
        State state_;
        State statePre_;
    };

public:

    static Input* GetInstance();
    void Finalize();
    Input() = default;
    ~Input() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(WinApp* winApp);

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    // ========================== キーボード ==========================//

    /// <summary>
    /// キーの押下をチェック
    /// </summary>
    /// <param name="keyNumber">キー番号( DIK_0 等)</param>
    /// <returns>押されているか</returns>
    bool PushKey(BYTE keyNumber);

    /// <summary>
    /// キーのトリガーをチェック
    /// </summary>
    /// <param name="keyNumber">キー番号( DIK_0 等)</param>
    /// <returns>トリガーか</returns>
    bool TriggerKey(BYTE keyNumber);

    /// <summary>
    /// 特定のキーが押され続けている時間を取得する
    /// </summary>
    /// <param name="keyNumber">キー番号</param>
    /// <returns>押され続けているフレーム数</returns>
    int32_t GetKeyPressDuration(BYTE keyNumber);

    /// <summary>
    /// 特定のキーが押されたかをバッファリングして判定する
    /// </summary>
    /// <param name="keyNumber">キー番号</param>
    /// <returns>バッファリングされている入力が存在するか</returns>
    bool BufferedKeyPress(BYTE keyNumber);

    /// <summary>
    /// 特定のキーの組み合わせをチェックする
    /// </summary>
    /// <param name="keyNumbers">キー番号の配列</param>
    /// <returns>全てのキーが押されているか</returns>
    bool AreKeysPressed(const std::vector<BYTE>& keyNumbers);

    // ========================== マウス ==========================//

    /// <summary>
    /// マウスの押下をチェック
    /// </summary>
    /// <param name="buttonNumber">マウスボタン番号(0:左,1:右,2:中,3~7:拡張マウスボタン)</param>
    /// <returns>押されているか</returns>
    bool IsPressMouse(int32_t buttonNumber) const;

    /// <summary>
    /// マウスのトリガーをチェック。押した瞬間だけtrueになる
    /// </summary>
    /// <param name="buttonNumber">マウスボタン番号(0:左,1:右,2:中,3~7:拡張マウスボタン)</param>
    /// <returns>トリガーか</returns>
    bool IsTriggerMouse(int32_t buttonNumber) const;

    /// <summary>
    /// 全マウス情報取得
    /// </summary>
    /// <returns>マウス情報</returns>
    const DIMOUSESTATE2& GetAllMouse() const;

    /// <summary>
    /// マウス移動量を取得
    /// </summary>
    /// <returns>マウス移動量</returns>
    MouseMove GetMouseMove();

    /// <summary>
    /// ホイールスクロール量を取得する
    /// </summary>
    /// <returns>ホイールスクロール量。奥側に回したら+。Windowsの設定で逆にしてたら逆</returns>
    int32_t GetWheel() const;

    /// <summary>
    /// マウスの位置を取得する（ウィンドウ座標系）
    /// </summary>
    /// <returns>マウスの位置</returns>
    const Vector2& GetMousePosition() const;

    /// <summary>
    /// マウスカーソルの表示・非表示を設定する
    /// </summary>
    /// <param name="isVisible">カーソルを表示するかどうか</param>
    void SetMouseCursorVisibility(bool isVisible);

    // ========================== コントローラー ==========================//

    /// <summary>
    /// ジョイスティックの現在の状態を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="out">ジョイスティックの現在の状態</param>
    /// <returns>正しく取得できた場合は true</returns>
    bool GetJoystickState(int32_t stickNo, DIJOYSTATE2& out) const;

    /// <summary>
    /// 前回のジョイスティックの状態を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="out">前回のジョイスティックの状態</param>
    /// <returns>正しく取得できた場合は true</returns>
    bool GetJoystickStatePrevious(int32_t stickNo, DIJOYSTATE2& out) const;

    /// <summary>
    /// XInput を使用したジョイスティックの現在の状態を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="out">ジョイスティックの現在の状態 (XINPUT_STATE)</param>
    /// <returns>正しく取得できた場合は true</returns>
    bool GetJoystickState(int32_t stickNo, XINPUT_STATE& out) const;

    /// <summary>
    /// 前回の XInput を使用したジョイスティックの状態を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="out">前回のジョイスティックの状態 (XINPUT_STATE)</param>
    /// <returns>正しく取得できた場合は true</returns>
    bool GetJoystickStatePrevious(int32_t stickNo, XINPUT_STATE& out) const;

    /// <summary>
    /// ジョイスティックのデッドゾーンを設定する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="deadZoneL">左スティックのデッドゾーン (0~32768)</param>
    /// <param name="deadZoneR">右スティックのデッドゾーン (0~32768)</param>
    void SetJoystickDeadZone(int32_t stickNo, int32_t deadZoneL, int32_t deadZoneR);

    /// <summary>
    /// ジョイスティックの振動を設定する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <param name="leftMotorSpeed">左モーターの強度 (0～65535)</param>
    /// <param name="rightMotorSpeed">右モーターの強度 (0～65535)</param>
    void SetJoystickVibration(int32_t stickNo, uint16_t leftMotorSpeed, uint16_t rightMotorSpeed);

    /// <summary>
    /// ジョイスティックのスティックの角度を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <returns>角度（0～360度）</returns>
    float GetJoystickAngle(int32_t stickNo);

    /// <summary>
    /// 接続されているジョイスティックの数を取得する
    /// </summary>
    /// <returns>接続されているジョイスティックの数</returns>
    size_t GetNumberOfJoysticks();

    /// <summary>
    /// ジョイスティックのキャリブレーションを行う
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    void CalibrateJoystick(int32_t stickNo);

    /// <summary>
    /// 指定されたボタンが押されているかチェック
    /// </summary>
    bool IsPadPressed(int32_t playerIndex, GamePadButton button) const;

    /// <summary>
    /// 指定されたボタンがトリガー（押した瞬間）かをチェック
    /// </summary>
    bool IsPadTriggered(int32_t playerIndex, GamePadButton button) const;



    /// <summary>
    /// 左スティックの入力を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <returns>スティック入力のベクトル</returns>
    Vector2 GetLeftStickInput(int32_t stickNo) const;

    /// <summary>
    /// 右スティックの入力を取得する
    /// </summary>
    /// <param name="stickNo">ジョイスティック番号</param>
    /// <returns>スティック入力のベクトル</returns>
    Vector2 GetRightStickInput(int32_t stickNo) const;


    /// <summary>
    /// コントローラーの接続確認
    /// </summary>
    /// <returns></returns>
    static bool IsControllerConnected();

    /// <summary>
    /// 左スティックの入力があるか
    /// </summary>
    /// <returns></returns>
    bool IsLeftStickMoving();

    /// <summary>
    /// 右スティックの入力があるか
    /// </summary>
    /// <returns></returns>
    bool IsRightStickMoving();

private:

    static Input* instance;
    Input(Input&) = delete;
    Input& operator = (Input&) = delete;
    ComPtr<IDirectInput8> directInput;
    ComPtr<IDirectInputDevice8> keyboard;
    // 全キーの入力情報を取得する
    BYTE key[256] = {};
    // 前回のキーの状態
    BYTE keyPre[256] = {};
    // キー押下時間を記録する
    std::chrono::steady_clock::time_point keyPressStart[256] = {};
    // WindowsAPI
    WinApp* winApp_ = nullptr;

    Microsoft::WRL::ComPtr<IDirectInputDevice8> devMouse_;
    DIMOUSESTATE2 mouse_;
    DIMOUSESTATE2 mousePre_;
    Vector2 mousePosition_;

    std::vector<Joystick> devJoysticks_;
};

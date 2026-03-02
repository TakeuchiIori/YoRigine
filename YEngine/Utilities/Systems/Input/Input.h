#pragma once

// C++
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800
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
	L_Stick = XINPUT_GAMEPAD_LEFT_THUMB,
	R_Stick = XINPUT_GAMEPAD_RIGHT_THUMB,
};

namespace YoRigine {
	///************************* 入力管理クラス *************************///
	/// キーボード・マウス・ゲームパッドの入力を統合管理する
	class Input
	{
	public:
		///************************* インナークラス *************************///

		struct MouseMove {
			LONG lX;
			LONG lY;
			LONG lZ;
		};

		enum class PadType {
			DirectInput,
			XInput,
		};

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
		///************************* 基本関数 *************************///

		static Input* GetInstance();
		void Finalize();
		Input() = default;
		~Input() = default;

		// 初期化
		void Initialize(WinApp* winApp);

		// 更新
		void Update();

	public:
		///************************* キーボード入力 *************************///

		bool PushKey(BYTE keyNumber);
		bool TriggerKey(BYTE keyNumber);
		int32_t GetKeyPressDuration(BYTE keyNumber);
		bool BufferedKeyPress(BYTE keyNumber);
		bool AreKeysPressed(const std::vector<BYTE>& keyNumbers);

	public:
		///************************* マウス入力 *************************///

		bool IsPressMouse(int32_t buttonNumber) const;
		bool IsTriggerMouse(int32_t buttonNumber) const;
		const DIMOUSESTATE2& GetAllMouse() const;
		MouseMove GetMouseMove();
		int32_t GetWheel() const;
		const Vector2& GetMousePosition() const;
		void SetMouseCursorVisibility(bool isVisible);

	public:
		///************************* ジョイスティック入力 *************************///

		bool GetJoystickState(int32_t stickNo, DIJOYSTATE2& out) const;
		bool GetJoystickStatePrevious(int32_t stickNo, DIJOYSTATE2& out) const;
		bool GetJoystickState(int32_t stickNo, XINPUT_STATE& out) const;
		bool GetJoystickStatePrevious(int32_t stickNo, XINPUT_STATE& out) const;
		void SetJoystickDeadZone(int32_t stickNo, int32_t deadZoneL, int32_t deadZoneR);
		void SetJoystickVibration(int32_t stickNo, uint16_t leftMotorSpeed, uint16_t rightMotorSpeed);
		float GetJoystickAngle(int32_t stickNo);
		size_t GetNumberOfJoysticks();
		void CalibrateJoystick(int32_t stickNo);
		bool IsPadPressed(int32_t playerIndex, GamePadButton button) const;
		bool IsPadTriggered(int32_t playerIndex, GamePadButton button) const;
		Vector2 GetLeftStickInput(int32_t stickNo) const;
		Vector2 GetRightStickInput(int32_t stickNo) const;
		static bool IsControllerConnected();
		bool IsLeftStickMoving();
		bool IsRightStickMoving();

		/// <summary>
		/// LT（左トリガー）が押されているか
		/// </summary>
		bool IsLTPressed(int32_t playerIndex = 0) const;

		/// <summary>
		/// RT（右トリガー）が押されているか
		/// </summary>
		bool IsRTPressed(int32_t playerIndex = 0) const;

		/// <summary>
		/// LT（左トリガー）が押された瞬間か
		/// </summary>
		bool IsLTTriggered(int32_t playerIndex = 0) const;

		/// <summary>
		/// RT（右トリガー）が押された瞬間か
		/// </summary>
		bool IsRTTriggered(int32_t playerIndex = 0) const;


		///************************* 左スティックのX/Y軸個別取得 *************************///
		float GetLeftStickX(int32_t stickNo) const;
		float GetLeftStickY(int32_t stickNo) const;
		float GetRightStickX(int32_t stickNo) const;
		float GetRightStickY(int32_t stickNo) const;

	private:
		///************************* メンバ変数 *************************///

		static Input* instance;
		Input(Input&) = delete;
		Input& operator=(Input&) = delete;

		ComPtr<IDirectInput8> directInput;
		ComPtr<IDirectInputDevice8> keyboard;
		BYTE key[256] = {};
		BYTE keyPre[256] = {};
		std::chrono::steady_clock::time_point keyPressStart[256] = {};
		WinApp* winApp_ = nullptr;

		Microsoft::WRL::ComPtr<IDirectInputDevice8> devMouse_;
		DIMOUSESTATE2 mouse_;
		DIMOUSESTATE2 mousePre_;
		Vector2 mousePosition_;

		std::vector<Joystick> devJoysticks_;
	};
}
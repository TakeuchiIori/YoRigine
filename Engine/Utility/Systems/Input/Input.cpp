#include "Input.h"

Input* Input::instance = nullptr;
Input* Input::GetInstance()
{
	if (instance == nullptr) {
		instance = new Input;
	}
	return instance;
}

void Input::Finalize()
{
	delete instance;
	instance = nullptr;
}

void Input::Initialize(WinApp* winApp)
{
	// 借りてきたwinAooのインスタンスを記録
	this->winApp_ = winApp;
	// DirectInputのインスタンス
	HRESULT result;
	result = DirectInput8Create(winApp_->Gethinstance(), DIRECTINPUT_HEADER_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));
	// キーボードデバイス生成

	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));
	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));
	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// マウスデバイス生成
	result = directInput->CreateDevice(GUID_SysMouse, &devMouse_, NULL);
	assert(SUCCEEDED(result));

	// マウスデータ形式のセット
	result = devMouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = devMouse_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

	// コントローラーデバイス生成

	// XInputパッドの準備（4つのパッドに対応）
	for (int i = 0; i < 4; ++i) {
		Joystick joystick = {};
		joystick.type_ = PadType::XInput;
		devJoysticks_.push_back(joystick);
	}
}

void Input::Update()
{
	HRESULT result;
	memcpy(keyPre, key, sizeof(key));
	// キーボード情報の取得開始
	result = keyboard->Acquire();
	result = keyboard->GetDeviceState(sizeof(key), key);

	// マウスの更新処理
	mousePre_ = mouse_; // 前回のマウス状態を保存
	result = devMouse_->Acquire();
	result = devMouse_->GetDeviceState(sizeof(DIMOUSESTATE2), &mouse_);

	// マウスの位置を更新する
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(winApp_->GetHwnd(), &point);
	mousePosition_.x = static_cast<float>(point.x);
	mousePosition_.y = static_cast<float>(point.y);

	// XInputコントローラーの更新処理
	for (int i = 0; i < 4; ++i) {
		Joystick& joystick = devJoysticks_[i];

		if (joystick.type_ == PadType::XInput) {
			joystick.statePre_ = joystick.state_; // 前回の状態を保存

			// XInputのステートを取得
			if (XInputGetState(i, &joystick.state_.xInput_) == ERROR_SUCCESS) {
				auto& gamepad = joystick.state_.xInput_.Gamepad;

				// スティックのデッドゾーン処理
				gamepad.sThumbLX = (std::abs(gamepad.sThumbLX) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? 0 : gamepad.sThumbLX;
				gamepad.sThumbLY = (std::abs(gamepad.sThumbLY) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? 0 : gamepad.sThumbLY;
				gamepad.sThumbRX = (std::abs(gamepad.sThumbRX) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? 0 : gamepad.sThumbRX;
				gamepad.sThumbRY = (std::abs(gamepad.sThumbRY) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? 0 : gamepad.sThumbRY;

				// トリガーのデッドゾーン処理
				gamepad.bLeftTrigger = (gamepad.bLeftTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 0 : gamepad.bLeftTrigger;
				gamepad.bRightTrigger = (gamepad.bRightTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 0 : gamepad.bRightTrigger;
			}
		}
	}
}

/// <summary>
/// キーの押下をチェック
/// </summary>
// 指定したキーを押していればtrueを返す
bool Input::PushKey(BYTE keyNumber)
{

	if (key[keyNumber]) {
		return true;
	}
	return false;
}

/// <summary>
/// キーのトリガーをチェック
/// </summary>
// 指定したキーを押した瞬間trueを返す
bool Input::TriggerKey(BYTE keyNumber)
{
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}
	return false;
}

/// <summary>
/// 特定のキーが押され続けている時間を取得する
/// </summary>
int32_t Input::GetKeyPressDuration(BYTE keyNumber)
{
	auto now = std::chrono::steady_clock::now();
	if (PushKey(keyNumber)) {
		if (keyPressStart[keyNumber] == std::chrono::steady_clock::time_point()) {
			keyPressStart[keyNumber] = now;
		}
		return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - keyPressStart[keyNumber]).count());
	}
	else {
		keyPressStart[keyNumber] = std::chrono::steady_clock::time_point();
		return 0;
	}
}

/// <summary>
/// 特定のキーが押されたかをバッファリングして判定する
/// </summary>
bool Input::BufferedKeyPress(BYTE keyNumber)
{
	return TriggerKey(keyNumber);
}

/// <summary>
/// 特定のキーの組み合わせをチェックする
/// </summary>
bool Input::AreKeysPressed(const std::vector<BYTE>& keyNumbers)
{
	for (BYTE keyNumber : keyNumbers) {
		if (!PushKey(keyNumber)) {
			return false;
		}
	}
	return true;
}

/// <summary>
/// マウスの押下をチェック
/// </summary>
/// <param name="buttonNumber">マウスボタン番号(0:左,1:右,2:中,3~7:拡張マウスボタン)</param>
/// <returns>押されているか</returns>
bool Input::IsPressMouse(int32_t buttonNumber) const {
	if (buttonNumber < 0 || buttonNumber >= 8) {
		return false;
	}
	return (mouse_.rgbButtons[buttonNumber] & 0x80) != 0;
}

/// <summary>
/// マウスのトリガーをチェック。押した瞬間だけtrueになる
/// </summary>
/// <param name="buttonNumber">マウスボタン番号(0:左,1:右,2:中,3~7:拡張マウスボタン)</param>
/// <returns>トリガーか</returns>
bool Input::IsTriggerMouse(int32_t buttonNumber) const {
	if (buttonNumber < 0 || buttonNumber >= 8) {
		return false;
	}
	return ((mouse_.rgbButtons[buttonNumber] & 0x80) != 0) && ((mousePre_.rgbButtons[buttonNumber] & 0x80) == 0);
}

/// <summary>
/// 全マウス情報取得
/// </summary>
/// <returns>マウス情報</returns>
const DIMOUSESTATE2& Input::GetAllMouse() const {
	return mouse_;
}

/// <summary>
/// マウス移動量を取得
/// </summary>
/// <returns>マウス移動量</returns>
Input::MouseMove Input::GetMouseMove() {
	MouseMove move = {};
	move.lX = mouse_.lX;
	move.lY = mouse_.lY;
	move.lZ = mouse_.lZ;
	return move;
}

/// <summary>
/// ホイールスクロール量を取得する
/// </summary>
/// <returns>ホイールスクロール量。奥側に回したら+。Windowsの設定で逆にしてたら逆</returns>
int32_t Input::GetWheel() const {
	return mouse_.lZ;
}

/// <summary>
/// マウスの位置を取得する（ウィンドウ座標系）
/// </summary>
/// <returns>マウスの位置</returns>
const Vector2& Input::GetMousePosition() const {
	return mousePosition_;
}

/// <summary>
/// マウスカーソルの表示・非表示を設定する
/// </summary>
void Input::SetMouseCursorVisibility(bool isVisible)
{
	ShowCursor(isVisible);
}

/// <summary>
/// ジョイスティックの現在の状態を取得する
/// </summary>
bool Input::GetJoystickState(int32_t stickNo, DIJOYSTATE2& out) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) {
		return false;
	}
	out = devJoysticks_[stickNo].state_.directInput_;
	return true;
}

/// <summary>
/// 前回のジョイスティックの状態を取得する
/// </summary>
bool Input::GetJoystickStatePrevious(int32_t stickNo, DIJOYSTATE2& out) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return false;
	if (devJoysticks_[stickNo].type_ != PadType::XInput) return false;

	out = devJoysticks_[stickNo].statePre_.directInput_;
	return true;
}

/// <summary>
/// XInput を使用したジョイスティックの現在の状態を取得する
/// </summary>
bool Input::GetJoystickState(int32_t stickNo, XINPUT_STATE& out) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) {
		return false;
	}
	out = devJoysticks_[stickNo].state_.xInput_;
	return true;
}

/// <summary>
/// 前回の XInput を使用したジョイスティックの状態を取得する
/// </summary>
bool Input::GetJoystickStatePrevious(int32_t stickNo, XINPUT_STATE& out) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return false;
	if (devJoysticks_[stickNo].type_ != PadType::XInput) return false;

	out = devJoysticks_[stickNo].statePre_.xInput_;
	return true;
}

/// <summary>
/// ジョイスティックのデッドゾーンを設定する
/// </summary>
void Input::SetJoystickDeadZone(int32_t stickNo, int32_t deadZoneL, int32_t deadZoneR)
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return;
	devJoysticks_[stickNo].deadZoneL_ = deadZoneL;
	devJoysticks_[stickNo].deadZoneR_ = deadZoneR;
}

/// <summary>
/// ジョイスティックの振動を設定する
/// </summary>
void Input::SetJoystickVibration(int32_t stickNo, uint16_t leftMotorSpeed, uint16_t rightMotorSpeed)
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return;
	if (devJoysticks_[stickNo].type_ == PadType::XInput) {
		XINPUT_VIBRATION vibration = {};
		vibration.wLeftMotorSpeed = leftMotorSpeed;
		vibration.wRightMotorSpeed = rightMotorSpeed;
		XInputSetState(stickNo, &vibration);
	}
}

/// <summary>
/// ジョイスティックのスティックの角度を取得する
/// </summary>
float Input::GetJoystickAngle(int32_t stickNo)
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return 0.0f;
	const DIJOYSTATE2& state = devJoysticks_[stickNo].state_.directInput_;
	float angle = atan2f(static_cast<float>(state.lY), static_cast<float>(state.lX)) * (180.0f / 3.14159265f);
	if (angle < 0) {
		angle += 360.0f;
	}
	return angle;
}

/// <summary>
/// 接続されているジョイスティックの数を取得する
/// </summary>
size_t Input::GetNumberOfJoysticks()
{
	return size_t();
}

/// <summary>
/// ジョイスティックのキャリブレーションを行う
/// </summary>
void Input::CalibrateJoystick(int32_t stickNo)
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) return;
	// キャリブレーション処理（例：初期位置の記録などを行う）
	devJoysticks_[stickNo].statePre_ = devJoysticks_[stickNo].state_;
}

bool Input::IsPadPressed(int32_t playerIndex, GamePadButton button) const {
	if (playerIndex < 0 || playerIndex >= devJoysticks_.size()) return false;

	const auto& joystick = devJoysticks_[playerIndex];
	if (joystick.type_ != PadType::XInput) return false;

	return (joystick.state_.xInput_.Gamepad.wButtons & static_cast<WORD>(button)) != 0;
}

bool Input::IsPadTriggered(int32_t playerIndex, GamePadButton button) const {
	if (playerIndex < 0 || playerIndex >= devJoysticks_.size()) return false;

	const auto& joystick = devJoysticks_[playerIndex];
	if (joystick.type_ != PadType::XInput) return false;

	return ((joystick.state_.xInput_.Gamepad.wButtons & static_cast<WORD>(button)) != 0) &&
		((joystick.statePre_.xInput_.Gamepad.wButtons & static_cast<WORD>(button)) == 0);
}

Vector2 Input::GetLeftStickInput(int32_t stickNo) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) {
		return { 0.0f, 0.0f };
	}
	const auto& joystick = devJoysticks_[stickNo];
	if (joystick.type_ != PadType::XInput) {
		return { 0.0f, 0.0f };
	}
	const auto& gamepad = joystick.state_.xInput_.Gamepad;
	return {
		static_cast<float>(gamepad.sThumbLX) / 32768.0f,
		static_cast<float>(gamepad.sThumbLY) / 32768.0f
	};
}

Vector2 Input::GetRightStickInput(int32_t stickNo) const
{
	if (stickNo < 0 || stickNo >= devJoysticks_.size()) {
		return { 0.0f, 0.0f };
	}
	const auto& joystick = devJoysticks_[stickNo];
	if (joystick.type_ != PadType::XInput) {
		return { 0.0f, 0.0f };
	}
	const auto& gamepad = joystick.state_.xInput_.Gamepad;
	return {
		static_cast<float>(gamepad.sThumbRX) / 32768.0f,
		static_cast<float>(gamepad.sThumbRY) / 32768.0f
	};
}

bool Input::IsControllerConnected()
{
	XINPUT_STATE state; ZeroMemory(&state, sizeof(XINPUT_STATE));
	// コントローラの状態を取得
	DWORD result = XInputGetState(0, &state);
	// コントローラが接続されている場合は true を返す
	return (result == ERROR_SUCCESS);
}

/// <summary>
/// 左スティックが動いているかを判定する
/// </summary>
/// <returns>左スティックが動いていれば true</returns>
bool Input::IsLeftStickMoving() {
	// ジョイスティック番号を固定（通常は0番目のコントローラーを対象とする）
	const int32_t stickNo = 0;

	// ジョイスティックの状態を取得
	XINPUT_STATE state;
	if (!GetJoystickState(stickNo, state)) {
		return false; // コントローラーが接続されていない場合
	}

	// 左スティックの入力値
	float leftStickX = state.Gamepad.sThumbLX / 32768.0f;
	float leftStickY = state.Gamepad.sThumbLY / 32768.0f;

	// 入力が閾値を超えているか判定
	const float deadZoneThreshold = 0.1f; // スティックの動きを無視する閾値
	return (std::fabs(leftStickX) > deadZoneThreshold || std::fabs(leftStickY) > deadZoneThreshold);
}

/// <summary>
/// 右スティックが動いているかを判定する
/// </summary>
/// <returns>右スティックが動いていれば true</returns>
bool Input::IsRightStickMoving() {
	// ジョイスティック番号を固定（通常は0番目のコントローラーを対象とする）
	const int32_t stickNo = 0;

	// ジョイスティックの状態を取得
	XINPUT_STATE state;
	if (!GetJoystickState(stickNo, state)) {
		return false; // コントローラーが接続されていない場合
	}

	// 右スティックの入力値
	float rightStickX = state.Gamepad.sThumbRX / 32768.0f;
	float rightStickY = state.Gamepad.sThumbRY / 32768.0f;

	// 入力が閾値を超えているか判定
	const float deadZoneThreshold = 0.1f; // スティックの動きを無視する閾値
	return (std::fabs(rightStickX) > deadZoneThreshold || std::fabs(rightStickY) > deadZoneThreshold);
}

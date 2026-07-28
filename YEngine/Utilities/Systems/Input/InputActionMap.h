#pragma once

// Engine
#include "Systems/Input./Input.h"

// Math
#include "Vector2.h"

// C++
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace YoRigine {

///************************* 列挙・構造体 *************************///

// 入力デバイス種別。最後に操作されたデバイスの判定に使う。
enum class InputDeviceKind {
  Keyboard,
  Gamepad,
};

// 軸に割り当てるスティック。None ならキーボードのみで駆動する。
enum class InputStickKind {
  None,
  Left,
  Right,
};

// ボタン系アクションのバインド。
// 同じアクションへ複数のキー／ボタンを割り当てられ、どれか1つでも成立すれば
// ON。
struct InputActionBinding {
  std::string name;
  std::vector<BYTE> keys; // DIK_* のリスト
  std::vector<GamePadButton> padButtons;
  bool leftTrigger = false;  // LT をこのアクションへ割り当てる
  bool rightTrigger = false; // RT をこのアクションへ割り当てる
};

// 軸（移動・視点）のバインド。スティックとキーボード4方向を同時に持てる。
struct InputAxisBinding {
  std::string name;
  InputStickKind stick = InputStickKind::Left;
  BYTE keyUp = 0; // 0 は未割り当て
  BYTE keyDown = 0;
  BYTE keyLeft = 0;
  BYTE keyRight = 0;
  float deadzone = 0.2f;
};

// 軸の評価結果。
// raw
// はデッドゾーン適用前の値。呼び出し側が独自の閾値を持つ場合はこちらを使う。
struct InputAxisValue {
  Vector2 value{0.0f, 0.0f};
  Vector2 raw{0.0f, 0.0f};
  float magnitude = 0.0f; // value の長さ（0〜1にクランプ）
  InputDeviceKind device = InputDeviceKind::Keyboard;
  bool isAnalog = false; // スティック由来なら true
};

///************************* アクションマップ *************************///

// アクション名からキー／パッド入力を引くための汎用マップ。
//
// このクラスはゲーム固有のアクション名を一切知らない。「攻撃」「回避」といった
// 名前はゲーム側（PlayerInput 等）が登録する。キーコンフィグやチュートリアルは
// 生のキーコードではなくこのクラスだけを見れば済むようになる。
class InputActionMap {
public:
  ///************************* 基本関数 *************************///

  static InputActionMap *GetInstance();

  // Input::Update()
  // の直後に毎フレーム呼ぶ。押下状態のキャッシュとエッジ検出を行う。
  void Update();

  // 使用するゲームパッドのインデックス。
  void SetPlayerIndex(int32_t index) { playerIndex_ = index; }
  int32_t GetPlayerIndex() const { return playerIndex_; }

  ///************************* バインド登録 *************************///

  // 同名のバインドが既にある場合は上書きする。
  void AddAction(const InputActionBinding &binding);
  void AddAxis(const InputAxisBinding &binding);
  void Clear();

  // JSON からバインドを読み込む。ファイルが無い／壊れている場合は false
  // を返し、
  // 登録済みのバインドはそのまま保持する（コード側の既定バインドが生き残る）。
  bool LoadFromFile(const std::string &path);
  bool SaveToFile(const std::string &path) const;

  // 軸のデッドゾーンだけを差し替える。ゲーム側の設定値を唯一の調整元にしたい場合に使う。
  void SetAxisDeadzone(std::string_view axis, float deadzone);

  ///************************* 問い合わせ *************************///

  bool IsPressed(std::string_view action) const;
  bool IsTriggered(std::string_view action) const;
  bool IsReleased(std::string_view action) const;

  InputAxisValue GetAxis(std::string_view axis) const;

  ///************************* ゲート *************************///

  // 無効化されたアクション／軸は、問い合わせが常に「入力なし」を返す。
  // チュートリアルで「回避だけ許可」といった制限を掛けるために使う。
  void SetEnabled(std::string_view name, bool enabled);
  bool IsEnabled(std::string_view name) const;
  void EnableAll();

  // allowed に含まれる名前だけを有効化し、それ以外を全て無効化する。
  void SetExclusivelyEnabled(const std::vector<std::string> &allowed);

  ///************************* 観測 *************************///

  // 最後に入力があったデバイス。入力が無い間は直前の値を保持する。
  InputDeviceKind LastDevice() const { return lastDevice_; }

  // アクションが押された瞬間に呼ばれる。チュートリアル等の購読用。
  // ゲートで無効化されたアクションは通知しない。
  void SetTriggerObserver(std::function<void(const std::string &)> observer) {
    triggerObserver_ = std::move(observer);
  }

  const std::vector<std::string> &GetActionNames() const {
    return actionNames_;
  }
  const std::vector<std::string> &GetAxisNames() const { return axisNames_; }
  const InputActionBinding *FindAction(std::string_view name) const;
  const InputAxisBinding *FindAxis(std::string_view name) const;

  ///************************* 名前変換 *************************///

  // 文字列とキーコード／パッドボタンの相互変換。JSON とエディタの両方で使う。
  static BYTE KeyCodeFromName(std::string_view name);
  static const char *KeyNameFromCode(BYTE code);
  static bool PadButtonFromName(std::string_view name, GamePadButton &out);
  static const char *PadButtonName(GamePadButton button);

private:
  ///************************* 内部処理 *************************///

  InputActionMap() = default;
  ~InputActionMap() = default;
  InputActionMap(const InputActionMap &) = delete;
  InputActionMap &operator=(const InputActionMap &) = delete;

  // string_view で unordered_map を引くための透過ハッシュ。
  struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept {
      return std::hash<std::string_view>{}(value);
    }
  };
  template <class T>
  using NameMap =
      std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

  // 押下状態のキャッシュ。Update() で1フレーム1回だけ評価する。
  struct ActionState {
    bool pressed = false;
    bool previous = false;
  };

  bool EvaluateAction(const InputActionBinding &binding,
                      bool &outFromGamepad) const;
  InputAxisValue EvaluateAxis(const InputAxisBinding &binding) const;

  ///************************* メンバ変数 *************************///

  NameMap<InputActionBinding> actions_;
  NameMap<InputAxisBinding> axes_;
  NameMap<ActionState> actionStates_;
  NameMap<InputAxisValue> axisValues_;

  // 登録順を保つ名前リスト。エディタ表示とチュートリアルの候補一覧に使う。
  std::vector<std::string> actionNames_;
  std::vector<std::string> axisNames_;

  // ゲートで無効化されている名前。アクション／軸を区別せず1つの集合で持つ。
  std::unordered_set<std::string, StringHash, std::equal_to<>> disabled_;

  std::function<void(const std::string &)> triggerObserver_;

  InputDeviceKind lastDevice_ = InputDeviceKind::Keyboard;
  int32_t playerIndex_ = 0;
};

} // namespace YoRigine

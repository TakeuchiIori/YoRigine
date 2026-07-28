#pragma once

// C++
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace YoRigine {

///************************* シグナル *************************///

// シグナル1件。name が識別子で、必要なら数値・文字列のペイロードを添える。
struct TutorialSignalData {
  std::string name;
  int intValue = 0;
  float floatValue = 0.0f;
  std::string stringValue;
};

// ゲーム内の出来事を名前で流す軽量なバス。
//
// ゲーム側は「チュートリアルのため」ではなく、自分の出来事として Emit する。
// チュートリアルはそれを購読するだけなので、両者は互いを知らないまま繋がる。
// エンジン内蔵の発火源（入力アクションなど）を接続しておけば、
// 「ボタンを押した」系のシグナルはゲーム側のコードを1行も書かずに流れてくる。
class TutorialSignal {
public:
  using Handle = uint32_t;
  using Listener = std::function<void(const TutorialSignalData &)>;

  ///************************* 基本関数 *************************///

  static TutorialSignal *GetInstance();

  // 発火。名前だけの単純なものと、ペイロード付きの2種類。
  static void Emit(const std::string &name);
  static void Emit(const TutorialSignalData &data);

  ///************************* 購読 *************************///

  // 戻り値の Handle を Unsubscribe へ渡すと購読を解除できる。
  Handle Subscribe(Listener listener);
  void Unsubscribe(Handle handle);

  ///************************* 内蔵発火源 *************************///

  // エンジン内蔵の発火源を接続する。初期化時に一度だけ呼ぶ。
  // 現在は InputActionMap のアクション押下を "action.triggered.<名前>"
  // として流す。
  void ConnectEngineSources();

  ///************************* 名前の一覧 *************************///

  // 発火実績のある名前。エディタの候補表示に使う。
  const std::vector<std::string> &GetKnownNames() const { return knownNames_; }

  // 実際に発火していない名前でも、候補として先に載せておける。
  void RegisterName(const std::string &name);

  // 入力アクションから作られるシグナル名。
  static std::string ActionTriggeredName(const std::string &actionName);

  // 接触から作られるシグナル名
  // （"collision.enter.<型名>" / "collision.exit.<型名>"）。
  static std::string ContactName(uint32_t collisionTypeId, bool entered);

private:
  ///************************* 内部構造 *************************///

  TutorialSignal() = default;
  ~TutorialSignal() = default;
  TutorialSignal(const TutorialSignal &) = delete;
  TutorialSignal &operator=(const TutorialSignal &) = delete;

  struct Entry {
    Handle handle = 0;
    Listener listener;
  };

  ///************************* メンバ変数 *************************///

  std::vector<Entry> listeners_;
  std::vector<std::string> knownNames_;
  Handle nextHandle_ = 1;
  bool engineSourcesConnected_ = false;
};

} // namespace YoRigine

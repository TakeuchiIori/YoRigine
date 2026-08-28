#pragma once

// Engine
#include "Systems/Tutorial/TutorialSignal.h"

// C++
#include <string>
#include <vector>

namespace YoRigine {

///************************* 条件の定義 *************************///

enum class TutorialConditionType {
  None,    // 未設定。従来の waitType へフォールバックする
  Signal,  // 指定シグナルが requiredCount 回届いたら成立
  Elapsed, // ステップ開始から seconds 秒経過したら成立
  Confirm, // 決定入力が押されたら成立
  All,     // 全ての子が成立したら成立
  Any,     // いずれかの子が成立したら成立
  Not,     // 先頭の子が不成立なら成立
};

// 完了条件・開始条件を表す木。
// 「攻撃を3回」「攻撃かつ敵に当てた」のような組み合わせを JSON
// で書けるようにする。
struct TutorialCondition {
  TutorialConditionType type = TutorialConditionType::None;
  std::string signalName;
  int requiredCount = 1;
  float seconds = 0.0f;
  std::vector<TutorialCondition> children;
};

///************************* 条件の評価状態 *************************///

// 条件ツリーと同じ形をした評価状態。シグナルの受信回数などを保持する。
//
// 定義側（TutorialCondition）は読み取り専用のデータで、受信回数のような
// 実行時の状態はこちらが持つ。ステップをやり直すときは Reset するだけでよい。
class TutorialConditionRuntime {
public:
  // 定義を束ね直し、受信回数などをリセットする。ステップ開始時に呼ぶ。
  // condition の寿命は、この Runtime を使い終わるまで保たれている必要がある。
  void Reset(const TutorialCondition &condition);

  // 定義との結び付きを解除する。
  void Clear();

  // 届いたシグナルを木全体へ配る。
  void OnSignal(const TutorialSignalData &signal);

  // 毎フレーム呼ぶ。経過時間と決定入力を取り込んでラッチする。
  // Confirm は押した瞬間しか来ないため、ここで成立を記録しておく必要がある。
  void Update(float elapsedSeconds, bool confirmTriggered);

  // 現在成立しているか。
  bool IsSatisfied() const;

  // 有効な定義が結び付いているか（type が None の場合は false）。
  bool HasDefinition() const;

private:
  const TutorialCondition *definition_ = nullptr;
  int receivedCount_ = 0;
  bool latched_ = false; // Elapsed / Confirm 用の成立フラグ
  std::vector<TutorialConditionRuntime> children_;
};

///************************* JSON *************************///

// nlohmann::json との相互変換。TutorialManager の保存・読込から使う。
const char *TutorialConditionTypeName(TutorialConditionType type);
TutorialConditionType TutorialConditionTypeFromName(const std::string &name);

} // namespace YoRigine

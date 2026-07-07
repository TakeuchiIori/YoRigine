#pragma once

#include "TriggerAction.h"

#include <json.hpp>
#include <memory>

// ============================================================
// TriggerActionFactory
//   JSON ノード (action 部分) から具象 TriggerAction を生成する。
//   "type" 文字列で分岐 (MVP では "OpenGate" のみ対応)。
//
//   未知の type / 必須パラメータ欠落の場合は nullptr を返す。
//   呼び出し側で nullptr チェック必須。
// ============================================================
class TriggerActionFactory {
public:
	static std::unique_ptr<TriggerAction> Create(const nlohmann::json& actionJson);
};

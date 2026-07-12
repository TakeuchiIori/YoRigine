#pragma once

#include "../AreaEffectBase.h"
#include <string>

// ============================================================
// バフエリア：円に入っている間だけ状態を付与し、出たら解除する。
//   Enter で status を有効化 / Exit で無効化する（tick 不要）。
//   使い方: Setup(center, radius); SetStatusId("AtkUp");
//   対象側は SetAreaStatus(statusId, enable) で効果を解釈する。
// ============================================================
class BuffArea : public AreaEffectBase {
public:
	void SetStatusId(const std::string& id) { statusId_ = id; }
	const std::string& GetStatusId() const  { return statusId_; }

protected:
	void OnEnter(IAreaEffectTarget* target) override {
		if (target) target->SetAreaStatus(statusId_, true);
	}
	void OnExit(IAreaEffectTarget* target) override {
		if (target) target->SetAreaStatus(statusId_, false);
	}

private:
	std::string statusId_ = "Buff";
};

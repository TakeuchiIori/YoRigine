#pragma once

#include "Loaders/Json/Use/AutoJson.h"

#include <string>

namespace YoRigine {

// 当たり判定システム全体の設定を編集・永続化するエディター。
// 個々のコライダー形状や配置はシーンエディター側に残し、
// BroadPhase／接触解決など全コライダー共通の設定だけを扱う。
class CollisionEditor
{
public:
	static CollisionEditor* GetInstance();

	void Initialize();
	void DrawImGui();

private:
	CollisionEditor();

	void ApplySettings();
	void SaveSettings();
	void LoadSettings();
	void ResetDefaults();

	static constexpr const char* kSettingsPath =
		"Resources/Json/Collision/CollisionSettings.json";

	AutoJson autoJson_;

	float broadPhaseCellSize_ = 2.5f;
	bool enableFrustumCulling_ = false;
	int resolveIterations_ = 3;
	int contactExitGraceFrames_ = 2;

	bool initialized_ = false;
	bool dirty_ = false;
	std::string status_;
};

} // namespace YoRigine

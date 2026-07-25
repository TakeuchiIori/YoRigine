#pragma once

#ifdef USE_IMGUI

#include "EventTriggerSystem.h"
#include "Vector3.h"

#include <functional>
#include <string>

namespace YoRigine { class Camera; }

// ============================================================
// EventTriggerEditor
//   EventTriggerSystem の ImGui 編集 UI。
//   Scene 側は Context を渡して呼ぶだけにし、UI 本体は Engine 側へ閉じ込める。
// ============================================================
class EventTriggerEditor {
public:
	struct Context {
		EventTriggerSystem* system = nullptr;
		std::string filePath;
		YoRigine::Camera* camera = nullptr;
		std::function<Vector3()> getPlacementPosition;
		std::function<void()> onGateOpened;
	};

	static void Draw(Context& context);
};

#endif

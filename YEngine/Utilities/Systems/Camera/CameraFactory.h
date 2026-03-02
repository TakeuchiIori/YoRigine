#pragma once
#include "Virtuals/VirtualCamera.h"
#include "Virtuals/DebugCamera/DebugCamera.h"
#include "Virtuals/FollowCamera/FollowCamera.h"
#include "Virtuals/ClearCamera/ClearCamera.h"

class CameraFactory
{
public:
	static std::shared_ptr<VirtualCamera> Create(const std::string& type) {
		if (type == "Debug") return std::make_shared<DebugCamera>();
		if (type == "Follow") return std::make_shared<FollowCamera>();
		if (type == "Clear") return std::make_shared<ClearCamera>();
		return nullptr;
	}

	// エディタの選択肢用
	static std::vector<std::string> GetTypeList() {
		return { "Debug", "Follow" ,"Clear"};
	}

	static std::string GetTypeName(std::shared_ptr<VirtualCamera> cam) {
		if (std::dynamic_pointer_cast<DebugCamera>(cam))  return "Debug";
		if (std::dynamic_pointer_cast<FollowCamera>(cam)) return "Follow";
		if (std::dynamic_pointer_cast<ClearCamera>(cam)) return "Clear";
		return "Unknown";
	}
};


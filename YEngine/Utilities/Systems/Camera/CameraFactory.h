#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Virtuals/VirtualCamera.h"
#include "Virtuals/DebugCamera/DebugCamera.h"
#include "Virtuals/FollowCamera/FollowCamera.h"
#include "Virtuals/ClearCamera/ClearCamera.h"
#include "Virtuals/TitleCamera/TitleCamera.h"

// ============================================================
// カメラファクトリークラス
// 文字列の型情報から対応するカメラインスタンスを生成する
// ============================================================
class CameraFactory
{
public:
	// ============================================================
	// インスタンスの生成
	// ============================================================
	static std::shared_ptr<VirtualCamera> Create(const std::string& type) {
		if (type == "Debug") return std::make_shared<DebugCamera>();
		if (type == "Follow") return std::make_shared<FollowCamera>();
		if (type == "Clear") return std::make_shared<ClearCamera>();
		if (type == "Title") return std::make_shared<TitleCamera>();
		return nullptr;
	}

	// ============================================================
	// エディタの選択肢用のリスト取得
	// ============================================================
	static std::vector<std::string> GetTypeList() {
		return { "Debug", "Follow" ,"Clear","Title" };
	}

	// ============================================================
	// カメラインスタンスからの型名取得（保存時に使用）
	// ============================================================
	static std::string GetTypeName(std::shared_ptr<VirtualCamera> cam) {
		if (std::dynamic_pointer_cast<DebugCamera>(cam))  return "Debug";
		if (std::dynamic_pointer_cast<FollowCamera>(cam)) return "Follow";
		if (std::dynamic_pointer_cast<ClearCamera>(cam)) return "Clear";
		if (std::dynamic_pointer_cast<TitleCamera>(cam)) return "Title";
		return "Unknown";
	}
};
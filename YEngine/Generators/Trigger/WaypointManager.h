#pragma once

#include "Vector3.h"

#include <string>
#include <unordered_map>
#include <cstdint>

class WaypointAction;

// ============================================================
// WaypointManager
//   ウェイポイント進行の集中管理（現在の目的地・ビーコン・名前解決）。
//   - WaypointAction が自身を「EventTrigger 名」で Register する。
//   - Activate(name) で目的地を切り替え、その位置にビーコン(VfxMesh)を出す。
//   - 現在地は Player の「向く方向」算出に使う（GetCurrentPosition）。
//
//   ※ EventTrigger / WaypointAction はシーン再入場で作り直されるので、
//     trigger ロード時に必ず Reset() を呼ぶこと。
// ============================================================
class WaypointManager {
public:
	static WaypointManager* GetInstance();

	// 登録・現在地・ビーコンを全消去（シーン入場時に呼ぶ）
	void Reset();

	// WaypointAction 側から自身を名前で登録 / 解除
	void Register(const std::string& name, WaypointAction* wp);
	void Unregister(WaypointAction* wp);

	// 指定名を現在の目的地にする（前のビーコンを止めて新しく出す）。空名 = 目的地なし（クリア）。
	void Activate(const std::string& name);

	// ビーコンの表示・非表示
	void SetBeaconVisible(bool visible);

	// 現在ウェイポイントがあるか / その位置（Player の向き用）
	bool    HasCurrent() const { return current_ != nullptr; }
	Vector3 GetCurrentPosition() const { return currentPos_; }

private:
	WaypointManager() = default;
	void StopBeacon();

	std::unordered_map<std::string, WaypointAction*> registry_;
	WaypointAction* current_ = nullptr;
	Vector3  currentPos_ = { 0.0f, 0.0f, 0.0f };
	uint32_t beaconId_   = 0;
	bool     hasBeacon_  = false;
};

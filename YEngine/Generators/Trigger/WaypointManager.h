#pragma once

#include "Vector3.h"

#include <string>
#include <unordered_map>
#include <cstdint>
#include <functional>

class WaypointAction;
class VfxMeshSpawner;

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

	// ============================================================
	// 依存先マネージャの注入 (DI)
	//   所有はしない (借用のみ)。Finalize() で nullptr に戻すのでダングリングポインタは残らない。
	// ============================================================
	void SetVfxMeshSpawner(VfxMeshSpawner* vfxMeshSpawner) { vfxMeshSpawner_ = vfxMeshSpawner; }

	// アプリ終了時に借用ポインタを手放す
	void Finalize();

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

	// ミッションUI ポーリング用アクセサ（現在の目標文・必要撃破数・現在撃破数）。
	// current_ が持つ WaypointAction の値をそのまま返す（実装は .cpp）。
	std::string GetCurrentMissionTitle() const;
	int         GetCurrentRequiredCount() const;
	int         GetCurrentProgress() const;
	// 目標が切り替わるたびに増える通し番号。UI側は変化を見て「新しい目標」を検知する。
	uint32_t    GetMissionSerial() const { return missionSerial_; }

	// ============================================================
	// ミッションUI連携（エンジンはゲームUIを知らないまま、コールバックで通知する）
	//   - Activated : 新しい目標がアクティブ化された（目標文・必要撃破数）
	//   - Progress  : 現在の目標の撃破数が進んだ（現在数・必要数）
	//   - Cleared   : 現在の目標を達成した（次が出る直前）
	//   GameUI が Initialize で設定し、破棄時に ClearMissionCallbacks() で外すこと。
	// ============================================================
	void SetOnMissionActivated(std::function<void(const std::string& title, int required)> cb) { onMissionActivated_ = std::move(cb); }
	void SetOnMissionProgress (std::function<void(int current, int required)> cb)              { onMissionProgress_  = std::move(cb); }
	void SetOnMissionCleared  (std::function<void()> cb)                                       { onMissionCleared_   = std::move(cb); }
	void ClearMissionCallbacks() { onMissionActivated_ = nullptr; onMissionProgress_ = nullptr; onMissionCleared_ = nullptr; }

	// WaypointAction から進捗・クリアを中継する
	void NotifyMissionProgress(int current, int required) { if (onMissionProgress_) onMissionProgress_(current, required); }
	void NotifyMissionCleared() { if (onMissionCleared_) onMissionCleared_(); }

private:
	WaypointManager() = default;
	void StopBeacon();

	std::unordered_map<std::string, WaypointAction*> registry_;
	WaypointAction* current_ = nullptr;
	Vector3  currentPos_ = { 0.0f, 0.0f, 0.0f };
	uint32_t beaconId_   = 0;
	bool     hasBeacon_  = false;
	uint32_t missionSerial_ = 0;  // 目標が切り替わるたびに +1（UIの新目標検知用）

	// 依存先マネージャ (借用のみ・非所有)。使用前に SetVfxMeshSpawner() で注入すること。
	VfxMeshSpawner* vfxMeshSpawner_ = nullptr;

	// ミッションUI通知コールバック（GameUI が設定・破棄時にクリア）
	std::function<void(const std::string&, int)> onMissionActivated_;
	std::function<void(int, int)>                onMissionProgress_;
	std::function<void()>                        onMissionCleared_;
};

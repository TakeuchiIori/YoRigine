#pragma once

#include "../TriggerAction.h"
#include "Vector3.h"

#include <string>

// ============================================================
// WaypointAction
//   「置いた地点を目的地として提示し、指定の敵グループを倒したら
//    次のウェイポイントへ連鎖する」トリガーアクション。
//   OpenGateAction と同じ撃破シグナル駆動の流儀。
//
//   - アクティブ化されると自分の位置にビーコン(VfxMesh)が出る（WaypointManager が担当）。
//   - requiredGroup を requiredCount 体倒すと nextWaypoint をアクティブ化する。
//   - startActive=true のものがシーン開始時の最初の目的地になる。
// ============================================================
class WaypointAction : public TriggerAction {
public:
	WaypointAction(std::string beaconEffect, std::string requiredGroup,
	               int requiredCount, std::string nextWaypoint, bool startActive);
	~WaypointAction() override;

	void OnAttach(EventTrigger* owner) override;
	void Update(float deltaTime) override;
	void NotifyEnemyDefeated(const std::string& group) override;

	nlohmann::json SerializeToJson() const override;
	std::string    GetTypeName() const override { return "Waypoint"; }
	bool           NeedsSpatialPlacement() const override { return true; }

	// WaypointManager から使う
	void    SetActive(bool a);
	bool    IsActive() const { return active_; }
	Vector3 GetWorldPosition() const;
	const std::string& GetBeaconEffect() const { return beaconEffect_; }
	float              GetBeaconScale()  const { return beaconScale_; }
	const std::string& GetNextWaypoint() const { return nextWaypoint_; }

	// エディタ用 setter / getter
	void SetBeaconEffect(const std::string& s) { beaconEffect_  = s; }
	void SetBeaconScale (float s)              { beaconScale_   = (s > 0.0f ? s : 1.0f); }
	void SetRequiredGroup(const std::string& s){ requiredGroup_ = s; }
	void SetRequiredCount(int n)               { requiredCount_ = (n > 0 ? n : 1); }
	void SetNextWaypoint (const std::string& s){ nextWaypoint_  = s; }
	void SetStartActive  (bool b)              { startActive_   = b; }
	const std::string& GetRequiredGroup() const { return requiredGroup_; }
	int                GetRequiredCount() const { return requiredCount_; }
	bool               IsStartActive()    const { return startActive_; }

private:
	std::string beaconEffect_;   // ビーコンに使う VfxMesh アセット名（LightVolume の柱など）
	std::string requiredGroup_;  // 倒す敵グループ（空 = どれでも）
	int         requiredCount_ = 1;
	std::string nextWaypoint_;   // クリア時にアクティブ化する次の EventTrigger 名（空 = 最終）
	bool        startActive_   = false;
	float       beaconScale_   = 1.0f;

	// 実行時状態
	bool active_         = false;  // 現在の目的地か（アクティブ時のみ撃破カウント）
	int  currentCount_   = 0;
	bool registered_     = false;  // WaypointManager へ登録済みか
	bool startTriggered_ = false;  // startActive の自動アクティブ化を一度だけ
};
